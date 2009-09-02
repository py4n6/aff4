/*
 * (c) Copyright 2001 - 2009 Hewlett-Packard Development Company, LP
 * [See end of file]
 */

package aff4.infomodel.aff4il;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.aff4il.parser.AFF4ILAntlrParser;
import aff4.infomodel.aff4il.parser.AFF4ILException;
import aff4.infomodel.aff4il.parser.AFF4ILParserEventHandler;
import aff4.infomodel.aff4il.parser.AntlrUtils;
import aff4.infomodel.aff4il.parser.AFF4ILParser;
import aff4.infomodel.datatypes.TypeMapper;
import aff4.infomodel.datatypes.DataType;

import antlr.collections.AST;


/**
 * A Handler for TriG parsing events which populates a NamedGraphSet. Based
 * on Jena's N3toRDF class.
 * 
 * @author		Andy Seaborne
 * @author Richard Cyganiak (richard@cyganiak.de)
 * @version 	$Id: NamedGraphSetPopulator.java,v 1.13 2009/04/22 17:23:50 jenpc Exp $
 */
public class NamedGraphSetPopulator implements AFF4ILParserEventHandler
{
    protected static Log logger = LogFactory.getLog( NamedGraphSetPopulator.class );
	static public boolean VERBOSE = false ;
	
	NamedGraphSet namedGraphSet;

//	// Maps URIref or a _:xxx bNode to a Resource
//	Map resourceRef = new HashMap() ;
//	// Maps URIref to Property
//	Map propertyRef = new HashMap() ;
    
    // A more liberal prefix mapping map.
    Map<String,String> myPrefixMapping = new HashMap<String,String>() ;
	
	// Well known namespaces
	
	static final String NS_rdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#" ;
    static final String NS_rdfs = "http://www.w3.org/2000/01/rdf-schema#" ;
	
    static final String NS_W3_log = "http://www.w3.org/2000/10/swap/log#" ;
    static final String LOG_IMPLIES = NS_W3_log+"implies" ; 
    static final String LOG_MEANS =   NS_W3_log+"means" ; 

    static final String XMLLiteralURI = "http://www.w3.org/1999/02/22-rdf-syntax-ns#XMLLiteral" ;

	String base = null ;
	final String anonPrefix = "_" ;

	private Node defaultGraphName;
	private Set<Node> previousGraphNames = new HashSet<Node>();

	public NamedGraphSetPopulator(NamedGraphSet ngs, String _base,
			String defaultGraphName)
	{
		this.namedGraphSet = ngs;
		this.base = _base ;
		this.defaultGraphName = this.defaultGraphName == null ?
				null : Node.createURI(defaultGraphName);
		if ( VERBOSE )
			System.out.println("N3toRDF: "+base) ;
	}
		
	
	public void startDocument() { }
	public void endDocument()   { }
	
	// When Jena exceptions are runtime, we will change this
	public void error(Exception ex, String message) 		{ throw new AFF4ILException(message) ; }
	public void error(String message) 						{ error(null, message) ; }
    
	public void warning(Exception ex, String message)       { logger.warn(message, ex) ; }
	public void warning(String message)						{ logger.warn(message) ; }
    
	public void deprecated(Exception ex, String message)	{ throw new AFF4ILException(message) ; }
	public void deprecated(String message)					{ deprecated(null, message) ; }
	
	public void startGraph(int line, AST graphName) {
		Node node = createGraphNameNode(line, graphName);

		if (this.previousGraphNames.contains(node)) {
			error("Line " + line + ": Graph names must be unique within file: " + node);
		}
		this.previousGraphNames.add(node);
		if (!this.namedGraphSet.containsGraph(node)) {
			// create the graph so it exists even if the graph pattern is empty
			this.namedGraphSet.createGraph(node);
		}
	}

	public void endGraph(int line, AST graphName) {
		// don't have to do anything
	}
	
	public void directive(int line, AST directive, AST[] args)
	{
		if ( directive.getType() == AFF4ILParser.AT_PREFIX )
		{
			// @prefix now
			if ( args[0].getType() != AFF4ILParser.QNAME )
			{
				error("Line "+line+": N3toRDF: Prefix directive does not start with a prefix! "+args[0].getText()+ "["+AFF4ILAntlrParser._tokenNames[args[0].getType()]+"]") ;
				return ;
			}
					
			String prefix = args[0].getText() ;
			if ( prefix.endsWith(":") )
				prefix = prefix.substring(0,prefix.length()-1) ;
				
			if ( args[1].getType() != AFF4ILParser.URIREF )
			{
				error("Line "+line+": N3toRDF: Prefix directive does not supply a URIref! "+args[1].getText()) ;
				return ;
			}
			
			String uriref = args[1].getText() ;
			if ( uriref.equals("") )
				uriref = base ;

			if ( uriref.equals("#") )
				uriref = base+"#" ;

			if ( VERBOSE )
				System.out.println(prefix+" => "+uriref) ;

            setPrefixMapping(prefix, uriref) ;
			return ;
		}
		
		warning("Line "+line+": N3toRDF: Directive not recongized and ignored: "+directive.getText()) ;
		return ;
	}
	
	
	public void quad(int line, AST subj, AST prop, AST obj, AST graphName)
	{
        // Syntax that reverses subject and object is done in the grammar

//		if ( context != null )
//			error("Line "+line+": N3toRDF: All statement are asserted - no formulae") ;
		
		try
		{
			// Converting N3 to RDF:
			// subject: must be a URIref or a bNode name
			// property: remove sugaring and then must be a URIref
			// object: can be a literal or a URIref or a bNode name
			// context must be zero (no formulae)

            // Lists: The parser creates list elements as sequences of triples:
            //       anon  list:first  ....
            //       anon  list:rest   resource
            // Where "resource" is nil for the last element of the list (generated first).

            // The properties are in a unique namespace to distinguish them
            // from lists encoded explicitly, not with the () syntax.

			int pType = prop.getType();
			String propStr = prop.getText();
            	Node pNode = null ;
			
			switch (pType)
			{
				case AFF4ILParser.ARROW_R :
					propStr = LOG_IMPLIES ;
					break;
				case AFF4ILParser.ARROW_MEANS :
					propStr = LOG_MEANS ;
					break;
				case AFF4ILParser.ARROW_L :
					// Need to reverse subject and object
					propStr = LOG_IMPLIES ;
					AST tmp = obj; obj = subj; subj = tmp;
					break;
				case AFF4ILParser.EQUAL :
					//propStr = NS_DAML + "equivalentTo";
					//propStr = damlVocab.equivalentTo().getURI() ;
					pNode = Node.createURI("aff4:sameAs");
					break;
				case AFF4ILParser.KW_A :
                    pNode = Node.createURI("aff4:type") ;
					break ;
				case AFF4ILParser.QNAME:
                    
                    if ( prop.getText().startsWith("_:") )
                        error("Line "+line+": N3toRDF: Can't have properties with labelled bNodes in RDF") ;
                    
                    String uriref = expandPrefix(propStr) ;
                    if ( uriref.equals(propStr) )
                    {
                        // Failed to expand ...
                        error("Line "+line+": N3toRDF: Undefined qname namespace: " + propStr);
                        return ;
                    }
                    pNode = Node.createURI(uriref) ;
                    break ;
				case AFF4ILParser.URIREF:
                    break ;

                // Literals, parser generated bNodes (other bnodes handled as QNAMEs)
                // and tokens that can't be properties.
                case AFF4ILParser.ANON:
                    error("Line "+line+": N3toRDF: Can't have anon. properties in RDF") ;
                    break ;
				default:
					error("Line "+line+": N3toRDF: Shouldn't see "+ prop +
								" at this point!") ;
                    break ;
			}

            // Didn't find an existing one above so make it ...
            if ( pNode == null )
                pNode = Node.createURI(expandRelativeURIRef(propStr));
            else
                propStr = pNode.getURI();

			Node sNode = createNode(line, subj);
            // Must be a resource
			if ( sNode instanceof Literal )
				error("Line "+line+": N3toRDF: Subject can't be a literal: " +subj.getText()) ;

			Node oNode = createNode(line, obj);
			
			Node gNode = createGraphNameNode(line, graphName);

			this.namedGraphSet.addQuad(new Quad(gNode, sNode, pNode, oNode));
		}
		catch (Exception rdfEx)
		{
			error("Line "+line+": JenaException: " + rdfEx);
		}
	}
    
	private Node createNode(int line, AST thing) 
	{
		//String tokenType = N3AntlrParser._tokenNames[thing.getType()] ;
		//System.out.println("Token type: "+tokenType) ;
		String text = thing.getText() ;
		switch (thing.getType())
		{
            case AFF4ILParser.NUMBER :
            	DataType xsdType = DataType.integer;
            	/*
            	
                if ( text.indexOf('.') >= 0 )
                    // The choice of XSD:double is for compatibility with N3/cwm.
                    xsdType = XSDDatatype.XSDdouble ;
                if ( text.indexOf('e') >= 0 || text.indexOf('E') >= 0)
                    xsdType = XSDDatatype.XSDdouble ;
                return Node.createLiteral(text, null, xsdType);
                */
            	return Node.createLiteral(text, null, xsdType);
                
			case AFF4ILParser.LITERAL :
				// Literals have three part: value (string), lang tag, datatype
                // Can't have a lang tag and a data type - if both, just use the datatype
                
                AST a1 = thing.getNextSibling() ;
                AST a2 = (a1==null?null:a1.getNextSibling()) ;
                AST datatype = null ;
                AST lang = null ;

                if ( a2 != null )
                {
                    if ( a2.getType() == AFF4ILParser.DATATYPE )
                        datatype = a2.getFirstChild() ;
                    else
                        lang = a2 ;
                }
                // First takes precedence over second.
                if ( a1 != null )
                {
                    if ( a1.getType() == AFF4ILParser.DATATYPE )
                        datatype = a1.getFirstChild() ;
                    else
                        lang = a1 ;
                }

                // Chop leading '@'
                String langTag = (lang!=null)?lang.getText().substring(1):null ;
                
                if ( datatype == null )
                    return Node.createLiteral(text, langTag, null) ;
                
                // If there is a datatype, it takes predence over lang tag.
                String typeURI = datatype.getText();

                if ( datatype.getType() != AFF4ILParser.QNAME &&
                     datatype.getType() != AFF4ILParser.URIREF )
                {
                    error("Line "+ line+ ": N3toRDF: Must use URIref or QName datatype URI: "
                            + text+ "^^"+ typeURI+"("+AFF4ILAntlrParser._tokenNames[datatype.getType()]+")");
                    return Node.createLiteral("Illegal literal: " + text + "^^" + typeURI, null, null);
 
                }
                
                // Can't have bNodes here so the code is slightly different for expansion
                
                if ( datatype.getType() == AFF4ILParser.QNAME )
                {
                    if (typeURI.startsWith("_:") || typeURI.startsWith("=:"))
                    {
                        error("Line "+ line+ ": N3toRDF: Can't use bNode for datatype URI: "
                                + text+ "^^"+ typeURI);
                        return Node.createLiteral("Illegal literal: " + text + "^^" + typeURI, null, null);
                    }

                    String typeURI2 = expandPrefix(typeURI) ;
                    if ( typeURI2 == typeURI )
                    {
                        error("Line "+line+": N3toRDF: Undefined qname namespace in datatype: " + typeURI);
                    }
                    
                    typeURI = typeURI2 ;
                }

                typeURI = expandRelativeURIRef(typeURI);
                DataType type = TypeMapper.getInstance().map(typeURI);
                return Node.createLiteral(text, null, type);
                //return Node.createLiteral(text, null, null);

			case AFF4ILParser.QNAME :
				// Is it a labelled bNode?
                // Check if _ has been defined.
			
                String uriref = expandPrefix(text) ;
                if ( uriref.equals(text) )
                {
                    error("Line "+line+": N3toRDF: Undefined qname namespace: " + text);
                    return null ;
                }
                return Node.createURI(expandRelativeURIRef(uriref));

            // Normal URIref - may be <> or <#>
            case AFF4ILParser.URIREF :
                return Node.createURI(expandRelativeURIRef(text));


            case AFF4ILParser.UVAR:
                error("Line "+line+": N3toRDF: Can't map variables to RDF: "+text) ;
                break ;

			default:
				error("Line "+line+": N3toRDF: Can't map to a resource or literal: "+AntlrUtils.ast(thing)) ;
                break ;
		}
		return null ;
	}

	private Node createGraphNameNode(int line, AST thing) {
		return thing == null
				? this.defaultGraphName
				: createNode(line, thing);
	}
			

    // Expand shorthand forms (not QNames) for URIrefs. Currently
	// deals only with the cases <>, <#> and <#...>, but should
	// probably also expand <file> to <http://example.com/base/file>
    private String expandRelativeURIRef(String text) {
        if (text.equals("") || text.startsWith("#")) {
            return this.base + text;
        }
        return text;
    }
    
    private void setPrefixMapping(String prefix, String uriref)
    {
        myPrefixMapping.put(prefix, uriref);
    }
    
    private String expandPrefix(String prefixed)
    {
        // Code from shared.impl.PrefixMappingImpl ...
        // Needed a copy as we store unchecked prefixes for N3.
        int colon = prefixed.indexOf( ':' );
        if (colon < 0) 
            return prefixed;
        else
        {
            String prefix = prefixed.substring( 0, colon );
            String uri = myPrefixMapping.get( prefix );
            if ( uri == null )
                return prefixed ;
            return uri + prefixed.substring( colon + 1 ) ;
        }
        // Not this - model may already have prefixes defined;
        // we allow "illegal" prefixes (e.g. starts with a number)
        // for compatibility 
        //return model.expandPrefix(prefix) ;
    }
}

/*
 *  (c) Copyright 2001 - 2009 Hewlett-Packard Development Company, LP
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

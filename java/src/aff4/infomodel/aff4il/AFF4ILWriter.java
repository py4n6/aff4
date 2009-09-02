/*
 * $Id: TriGWriter.java,v 1.10 2009/06/09 16:20:51 hartig Exp $
 */
package aff4.infomodel.aff4il;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.io.Writer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import aff4.infomodel.Literal;
import aff4.infomodel.NamedGraph;
import aff4.infomodel.NamedGraphSet;
import aff4.infomodel.Node;
import aff4.infomodel.Quad;
import aff4.infomodel.QuadList;
import aff4.infomodel.QuadStore;
import aff4.infomodel.QueryTools;
import aff4.infomodel.Resource;

public class AFF4ILWriter   {
	private Writer writer;
	private NamedGraph currentGraph;
	private PrettyNamespacePrefixMaker prefixMaker;
	private Map<String,String> customPrefixes = new HashMap<String,String>();
	
	int indent = 0;
	QuadStore qs = null;
	

	void writeResource(StringBuffer out, Resource r) {
		for (int i = 0 ; i < indent ; i++) {
			out.append("\t");
		}
		if (this.prefixMaker.isPrefixable(r)) {
			Resource prefixedResource = this.prefixMaker.toPrefixedResource(r);
			out.append(prefixedResource.getURI());
		} else {
			out.append("<");
			out.append(r.getURI());
			out.append("> ");
		}
	}
	
	void writeLiteral(StringBuffer out, Literal l) {
		for (int i = 0 ; i < indent ; i++) {
			out.append("\t");
		}
		out.append("\"");
		out.append(l.getLexicalForm());
		if (l.getDatatypeURI() != null) {
			out.append("\"^^");
			Resource r = l.getDatatypeURI();
			if (this.prefixMaker.isPrefixable(r)) {
				Resource prefixedResource = this.prefixMaker.toPrefixedResource(r);
				out.append(prefixedResource.getNameSpace());
				out.append(":");
				out.append(prefixedResource.getLocalName());
			} else {
				out.append("<");
				out.append(r.getURI());
				out.append("> ");
			}
		} else {
			out.append("\"");
		}
	}
	
	public void write(QuadStore qs, Writer out, String baseURI) throws IOException {
		this.writer = new BufferedWriter(out);

		QuadList allTriples = qs.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY);

		this.prefixMaker = new PrettyNamespacePrefixMaker(allTriples);
		this.prefixMaker.setBaseURI(baseURI);
		this.prefixMaker.addDefaultNamespace("rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
		this.prefixMaker.addDefaultNamespace("xsd", "http://www.w3.org/2001/XMLSchema#");
		this.prefixMaker.addDefaultNamespace("dc", "http://purl.org/dc/elements/1.1/");
		this.prefixMaker.addDefaultNamespace("dcterms", "http://purl.org/dc/terms/");
		this.prefixMaker.addDefaultNamespace("foaf", "http://xmlns.com/foaf/0.1/");
		this.prefixMaker.addDefaultNamespace("contact", "http://www.w3.org/2000/10/swap/pim/contact#");

		Iterator<String> it = this.customPrefixes.keySet().iterator();
		while (it.hasNext()) {
			String prefix = (String) it.next();
			String uri = (String) this.customPrefixes.get(prefix);
			this.prefixMaker.addNamespace(prefix, uri);
		}
		
		Set<Resource> graphs = QueryTools.getUniqueGraphs(qs.query(Node.ANY, Node.ANY, Node.ANY, Node.ANY));
		
		StringBuffer namespacePortion = new StringBuffer();
		StringBuffer graphPortion = new StringBuffer();
		
		for (Resource graph : graphs) {
			indent = 0;
			writeResource(graphPortion, graph);
			
			graphPortion.append(" {\r\n");
			
			List<Resource> graphSubjects = QueryTools.getUniqueSubjects(qs.query(graph, Node.ANY, Node.ANY , Node.ANY ));
			
			for (Resource subject : graphSubjects) {
				indent = 1;
				writeResource(graphPortion, subject);
				
				int count = 0;
				QuadList properties = qs.query(graph, subject, Node.ANY, Node.ANY);
				for (Quad property : properties) {
					if (count >= 1) {
						graphPortion.append(" ;\r\n");
					}
					
					indent = 2;
					writeResource(graphPortion, property.getPredicate());
					
					Node o = property.getObject();
					indent = 1;
					if (o instanceof Literal) {
						Literal l = (Literal)o;
						graphPortion.append("\t");
						writeLiteral(graphPortion, l);
					} else {

						writeResource(graphPortion, (Resource)o);
					}
					
					count += 1;
				}
				if (count > 0) {
					graphPortion.append(" .\r\n");
				}
				
			}
			graphPortion.append("} \r\n");	
			
		}
		
		this.prefixMaker.writeNamespaces(namespacePortion);
		this.writer.write(namespacePortion.toString());
		this.writer.write(graphPortion.toString());
		this.writer.close();

	}

	/**
	 * Writes a NamedGraphSet to an OutputStream. The base URI is optional.
	 * @throws IOException 
	 */
	public void write(QuadStore set, OutputStream out, String baseURI) throws IOException {
		try {
			write(set, new OutputStreamWriter(out, "utf-8"), baseURI);
		} catch (UnsupportedEncodingException ueex) {
			// UTF-8 is always supported
		}
	}

	/**
	 * Adds a custom namespace prefix.
	 * @param prefix The namespace prefix
	 * @param namespaceURI The full namespace URI
	 */
	public void addNamespace(String prefix, String namespaceURI) {
		this.customPrefixes.put(prefix, namespaceURI);
	}



}

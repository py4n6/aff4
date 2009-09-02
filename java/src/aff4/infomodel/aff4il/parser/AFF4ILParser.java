/*
 * (c) Copyright 2001 - 2009 Hewlett-Packard Development Company, LP
 * [See end of file]
 */

package aff4.infomodel.aff4il.parser;

import java.io.Reader;


import antlr.RecognitionException;
import antlr.TokenStreamException;

/**
 * The formal interface to the TriG parser.  Wraps up the antlr parser and lexer.
 * @author		Andy Seaborne
 * @author Richard Cyganiak (richard@cyganiak.de)
 * @version 	$Id: TriGParser.java,v 1.4 2009/02/20 08:09:52 hartig Exp $
 */
public class AFF4ILParser implements AFF4ILAntlrParserTokenTypes {
	private AFF4ILAntlrLexer lexer = null ;
	private AFF4ILAntlrParser parser = null ;
	
	public AFF4ILParser(Reader r, AFF4ILParserEventHandler h) {
		this.lexer = new AFF4ILAntlrLexer(r);
		this.parser = createParser(h);
    }
    
	/**
	 * Runs the parsing process by calling the top level parser rule
	 */
	public void parse() throws RecognitionException, TokenStreamException {
		this.parser.document();
	}
	
	private AFF4ILAntlrParser createParser(AFF4ILParserEventHandler handler) {
		AFF4ILAntlrParser result = new AFF4ILAntlrParser(this.lexer);
		result.setEventHandler(handler);
		result.setLexer(this.lexer);
		return result;
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

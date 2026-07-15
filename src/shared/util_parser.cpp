/********************************************************************
*
*
*   Modular Parser / Tokenizer Implementation:
*
*
********************************************************************/
#include "shared/shared.h"

#ifdef __cplusplus
#include "shared/util/util_parser.h"

/**
*
*	shared_tokenizer_t
*
**/

/**
*	@brief	Constructor
**/
shared_tokenizer_t::shared_tokenizer_t( const std::string_view source, const shared_parser_mode_e mode ) : m_source( source ), m_mode( mode ) {
}

/**
*	@brief	Reports an error at the current tokenizer position.
**/
void shared_tokenizer_t::ReportError( const std::string& message ) {
	ReportError( m_currentLine, m_currentColumn, message );
}

/**
*	@brief	Reports an error at a specific position.
**/
void shared_tokenizer_t::ReportError( int32_t line, int32_t col, const std::string& message ) {
	m_errors.push_back( "Error at line " + std::to_string( line ) + " col " + std::to_string( col ) + ": " + message );
}

/**
*	@brief	Advances the position in the source by `count` characters.
**/
void shared_tokenizer_t::Advance( size_t count ) {
	for ( size_t i = 0; i < count; i++ ) {
		if ( IsAtEnd() ) {
			break;
		}
		if ( m_source[ m_position ] == '\n' ) {
			m_currentLine++;
			m_currentColumn = 1;
		} else {
			m_currentColumn++;
		}
		m_position++;
	}
}

/**
*	@brief	Peeks at a character at the current position + `offset`.
**/
char shared_tokenizer_t::PeekChar( size_t offset ) const {
	if ( m_position + offset >= m_source.length() ) {
		return '\0';
	}
	return m_source[ m_position + offset ];
}

/**
*	@brief	Returns true if we reached the end of the source.
**/
bool shared_tokenizer_t::IsAtEnd() const {
	return m_position >= m_source.length();
}

/**
*	@brief	Skips all whitespace characters and handles C/C++ style comments.
**/
void shared_tokenizer_t::SkipWhitespaceAndComments() {
	bool trackNewlines = ( m_mode == shared_parser_mode_e::MODE_Q3_SHADER || m_mode == shared_parser_mode_e::MODE_Q2RTX_MATERIALS );

	while ( !IsAtEnd() ) {
		char c = PeekChar();

		if ( Q_isspace( c ) ) {
			if ( trackNewlines && ( c == '\n' || c == '\r' ) ) {
				// We don't skip newlines in these modes
				break;
			}
			Advance();
		} else if ( c == '/' && PeekChar( 1 ) == '/' ) {
			// Line comment
			Advance( 2 );
			while ( !IsAtEnd() && PeekChar() != '\n' ) {
				Advance();
			}
		} else if ( c == '/' && PeekChar( 1 ) == '*' ) {
			// Block comment
			Advance( 2 );
			while ( !IsAtEnd() ) {
				if ( PeekChar() == '*' && PeekChar( 1 ) == '/' ) {
					Advance( 2 );
					break;
				}
				Advance();
			}
		} else {
			break;
		}
	}
}

/**
*	@brief	Reads a string literal enclosed in double quotes.
**/
shared_token_t shared_tokenizer_t::ReadString() {
	shared_token_t token;
	token.type = shared_token_type_e::TOKEN_STRING;
	token.line = m_currentLine;
	token.column = m_currentColumn;

	// Skip the opening quote
	Advance();

	size_t startPos = m_position;

	while ( !IsAtEnd() && PeekChar() != '"' ) {
		// Handle escape sequences if necessary (simplified here)
		if ( PeekChar() == '\\' && !IsAtEnd() ) {
			Advance( 2 );
		} else {
			Advance();
		}
	}

	if ( IsAtEnd() ) {
		ReportError( token.line, token.column, "Unterminated string literal." );
		token.type = shared_token_type_e::TOKEN_ERROR;
	}

	token.value = m_source.substr( startPos, m_position - startPos );

	// Skip the closing quote
	if ( !IsAtEnd() && PeekChar() == '"' ) {
		Advance();
	}

	return token;
}

/**
*	@brief	Reads a numeric literal (integers and floats).
**/
shared_token_t shared_tokenizer_t::ReadNumber() {
	shared_token_t token;
	token.type = shared_token_type_e::TOKEN_NUMBER;
	token.line = m_currentLine;
	token.column = m_currentColumn;

	size_t startPos = m_position;

	// Optional sign
	if ( PeekChar() == '-' || PeekChar() == '+' ) {
		Advance();
	}

	while ( !IsAtEnd() && Q_isdigit( PeekChar() ) ) {
		Advance();
	}

	// Decimal part
	if ( PeekChar() == '.' ) {
		Advance();
		while ( !IsAtEnd() && Q_isdigit( PeekChar() ) ) {
			Advance();
		}
	}

	token.value = m_source.substr( startPos, m_position - startPos );
	return token;
}

/**
*	@brief	Reads an identifier (keywords, property names, etc).
**/
shared_token_t shared_tokenizer_t::ReadIdentifier() {
	shared_token_t token;
	token.type = shared_token_type_e::TOKEN_IDENTIFIER;
	token.line = m_currentLine;
	token.column = m_currentColumn;

	size_t startPos = m_position;

	while ( !IsAtEnd() ) {
		char c = PeekChar();
		if ( Q_isalnum( c ) || c == '_' || c == '-' || c == '$' ) {
			Advance();
		} else {
			break;
		}
	}

	token.value = m_source.substr( startPos, m_position - startPos );
	return token;
}

/**
*	@brief	Retrieves the next token in the stream.
**/
shared_token_t shared_tokenizer_t::NextToken() {
	if ( m_peekedToken.has_value() ) {
		shared_token_t token = m_peekedToken.value();
		m_peekedToken.reset();
		return token;
	}

	SkipWhitespaceAndComments();

	shared_token_t token;
	token.line = m_currentLine;
	token.column = m_currentColumn;

	if ( IsAtEnd() ) {
		token.type = shared_token_type_e::TOKEN_EOF;
		return token;
	}

	char c = PeekChar();

	if ( c == '\n' || c == '\r' ) {
		// Only reached if mode is Q3 or Q2RTX and tracking newlines
		token.type = shared_token_type_e::TOKEN_NEWLINE;
		token.value = m_source.substr( m_position, 1 );
		Advance();
		// Skip consecutive newlines to avoid spamming empty tokens
		while ( !IsAtEnd() && ( PeekChar() == '\n' || PeekChar() == '\r' ) ) {
			Advance();
		}
		return token;
	} else if ( c == '"' ) {
		return ReadString();
	} else if ( Q_isdigit( c ) || ( ( c == '-' || c == '+' ) && Q_isdigit( PeekChar( 1 ) ) ) || ( c == '.' && Q_isdigit( PeekChar( 1 ) ) ) ) {
		return ReadNumber();
	} else if ( Q_isalpha( c ) || c == '_' || c == '$' ) {
		return ReadIdentifier();
	} else {
		// Punctuation (braces, brackets, colons, etc)
		token.type = shared_token_type_e::TOKEN_PUNCTUATION;
		token.value = m_source.substr( m_position, 1 );
		Advance();
		return token;
	}
}

/**
*	@brief	Peeks the next token.
**/
shared_token_t shared_tokenizer_t::PeekToken() {
	if ( !m_peekedToken.has_value() ) {
		m_peekedToken = NextToken();
	}
	return m_peekedToken.value();
}


/**
*
*	shared_parser_t
*
**/

/**
*	@brief	Constructor
**/
shared_parser_t::shared_parser_t( const std::string_view source, const shared_parser_mode_e mode ) : m_tokenizer( source, mode ), m_mode( mode ) {
}

/**
*	@brief	Reports an error at the position of the given token.
**/
void shared_parser_t::ReportError( const shared_token_t& token, const std::string& message ) {
	m_tokenizer.ReportError( token.line, token.column, message );
}

/**
*	@brief	Parses a block encapsulated by '{' and '}'
**/
shared_ast_node_t shared_parser_t::ParseBlock() {
	shared_ast_node_t blockNode;
	blockNode.type = shared_ast_node_type_e::NODE_BLOCK;

	// Consume the '{'
	m_tokenizer.NextToken();

	while ( true ) {
		shared_token_t token = m_tokenizer.PeekToken();
		
		if ( token.type == shared_token_type_e::TOKEN_EOF ) {
			ReportError( token, "Unexpected EOF while looking for closing brace '}'." );
			break; // Unexpected EOF, but we handle it gracefully by breaking
		}

		if ( token.type == shared_token_type_e::TOKEN_PUNCTUATION && token.value == "}" ) {
			m_tokenizer.NextToken(); // Consume '}'
			break;
		}

		if ( token.type == shared_token_type_e::TOKEN_PUNCTUATION && token.value == "{" ) {
			blockNode.children.push_back( ParseBlock() );
			continue;
		} 
		
		// Skip empty newlines in block scopes
		if ( token.type == shared_token_type_e::TOKEN_NEWLINE ) {
			m_tokenizer.NextToken();
			continue;
		}

		// Handle properties based on mode
		if ( m_mode == shared_parser_mode_e::MODE_CSS && token.type == shared_token_type_e::TOKEN_IDENTIFIER ) {
			shared_token_t keyToken = m_tokenizer.NextToken();
			shared_token_t nextToken = m_tokenizer.PeekToken();

			if ( nextToken.type == shared_token_type_e::TOKEN_PUNCTUATION && nextToken.value == ":" ) {
				// It's a CSS property
				m_tokenizer.NextToken(); // Consume ':'

				shared_ast_node_t propNode;
				propNode.type = shared_ast_node_type_e::NODE_PROPERTY;
				propNode.token = keyToken;
				
				std::string valueStr;
				while ( true ) {
					shared_token_t vTok = m_tokenizer.PeekToken();
					if ( vTok.type == shared_token_type_e::TOKEN_EOF || ( vTok.type == shared_token_type_e::TOKEN_PUNCTUATION && vTok.value == "}" ) ) {
						ReportError( vTok, "Missing ';' for property." );
						break;
					}
					if ( vTok.type == shared_token_type_e::TOKEN_PUNCTUATION && vTok.value == ";" ) {
						m_tokenizer.NextToken(); // Consume ';'
						break;
					}
					if ( !valueStr.empty() && vTok.type != shared_token_type_e::TOKEN_PUNCTUATION ) {
						valueStr += " ";
					}
					valueStr += std::string( m_tokenizer.NextToken().value );
				}
				propNode.string_value = valueStr;
				blockNode.children.push_back( propNode );
				continue;
			} else {
				// Not a property, just add the identifier token
				shared_ast_node_t childNode;
				childNode.type = shared_ast_node_type_e::NODE_TOKEN;
				childNode.token = keyToken;
				blockNode.children.push_back( childNode );
				continue;
			}
		} else if ( m_mode == shared_parser_mode_e::MODE_Q3_SHADER && token.type == shared_token_type_e::TOKEN_IDENTIFIER ) {
			shared_token_t keyToken = m_tokenizer.NextToken();

			shared_ast_node_t propNode;
			propNode.type = shared_ast_node_type_e::NODE_PROPERTY;
			propNode.token = keyToken;
			
			std::string valueStr;
			while ( true ) {
				shared_token_t vTok = m_tokenizer.PeekToken();
				if ( vTok.type == shared_token_type_e::TOKEN_EOF || vTok.type == shared_token_type_e::TOKEN_NEWLINE || ( vTok.type == shared_token_type_e::TOKEN_PUNCTUATION && vTok.value == "}" ) ) {
					if ( vTok.type == shared_token_type_e::TOKEN_NEWLINE ) {
						m_tokenizer.NextToken(); // Consume newline
					}
					break;
				}
				if ( !valueStr.empty() && vTok.type != shared_token_type_e::TOKEN_PUNCTUATION && valueStr.back() != '"' ) {
					valueStr += " ";
				}
				valueStr += std::string( m_tokenizer.NextToken().value );
			}
			propNode.string_value = valueStr;
			blockNode.children.push_back( propNode );
			continue;
		}

		// Regular token fallback
		shared_ast_node_t childNode;
		childNode.type = shared_ast_node_type_e::NODE_TOKEN;
		childNode.token = m_tokenizer.NextToken();
		blockNode.children.push_back( childNode );
	}

	return blockNode;
}

/**
*	@brief	Parses the entire document into a root node.
**/
shared_ast_node_t shared_parser_t::Parse() {
	shared_ast_node_t root;
	root.type = shared_ast_node_type_e::NODE_ROOT;

	if ( m_mode == shared_parser_mode_e::MODE_Q2RTX_MATERIALS ) {
		// Q2RTX lacks { } block scopes, headers end with ':'
		shared_ast_node_t* currentBlock = nullptr;

		while ( true ) {
			shared_token_t token = m_tokenizer.PeekToken();
			if ( token.type == shared_token_type_e::TOKEN_EOF ) {
				break;
			}
			if ( token.type == shared_token_type_e::TOKEN_NEWLINE ) {
				m_tokenizer.NextToken();
				continue;
			}

			// Read a full line
			std::vector<shared_token_t> lineTokens;
			while ( true ) {
				shared_token_t t = m_tokenizer.NextToken();
				if ( t.type == shared_token_type_e::TOKEN_EOF || t.type == shared_token_type_e::TOKEN_NEWLINE ) {
					break;
				}
				lineTokens.push_back( t );
			}

			if ( lineTokens.empty() ) continue;

			// Check if line is a header (ends with ':')
			bool isHeader = ( lineTokens.back().type == shared_token_type_e::TOKEN_PUNCTUATION && lineTokens.back().value == ":" );
			
			if ( isHeader ) {
				// Accumulate header string
				std::string headerStr;
				for ( size_t i = 0; i < lineTokens.size() - 1; i++ ) {
					headerStr += std::string( lineTokens[ i ].value );
				}
				
				// Create a new block
				shared_ast_node_t blockNode;
				blockNode.type = shared_ast_node_type_e::NODE_BLOCK;
				
				// We attach the selector as a token node inside the root before the block
				shared_ast_node_t selectorNode;
				selectorNode.type = shared_ast_node_type_e::NODE_TOKEN;
				selectorNode.token = lineTokens[ 0 ];
				selectorNode.token.value = headerStr; // slightly hacky but works for the AST
				selectorNode.token.type = shared_token_type_e::TOKEN_IDENTIFIER;
				
				root.children.push_back( selectorNode );
				root.children.push_back( blockNode );
				
				// Point currentBlock to the newly added block
				currentBlock = &root.children.back();
			} else if ( currentBlock && !lineTokens.empty() ) {
				// It's a property
				shared_ast_node_t propNode;
				propNode.type = shared_ast_node_type_e::NODE_PROPERTY;
				propNode.token = lineTokens[ 0 ];
				
				std::string valueStr;
				for ( size_t i = 1; i < lineTokens.size(); i++ ) {
					if ( i > 1 && lineTokens[ i ].type != shared_token_type_e::TOKEN_PUNCTUATION ) {
						valueStr += " ";
					}
					valueStr += std::string( lineTokens[ i ].value );
				}
				propNode.string_value = valueStr;
				currentBlock->children.push_back( propNode );
			} else if ( !currentBlock ) {
				ReportError( lineTokens[ 0 ], "Encountered property without a preceding block header." );
			}
		}
		return root;
	}

	// Generic, CSS, and Q3 Shader top-level parsing
	while ( true ) {
		shared_token_t token = m_tokenizer.PeekToken();
		
		if ( token.type == shared_token_type_e::TOKEN_EOF ) {
			break;
		}
		
		if ( token.type == shared_token_type_e::TOKEN_NEWLINE ) {
			m_tokenizer.NextToken();
			continue;
		}

		if ( token.type == shared_token_type_e::TOKEN_PUNCTUATION && token.value == "{" ) {
			root.children.push_back( ParseBlock() );
		} else {
			shared_ast_node_t childNode;
			childNode.type = shared_ast_node_type_e::NODE_TOKEN;
			childNode.token = m_tokenizer.NextToken();
			root.children.push_back( childNode );
		}
	}

	return root;
}

/**
*	@brief	Parses the string_value as a float. If it fails, reports an error to the parser.
**/
float shared_ast_node_t::GetPropertyFloat( shared_parser_t& parser, const float defaultValue ) const {
	if ( string_value.empty() ) {
		parser.ReportError( token, "Property '" + std::string( token.value ) + "' requires a float value." );
		return defaultValue;
	}
	try {
		return std::stof( string_value );
	} catch ( ... ) {
		parser.ReportError( token, "Failed to parse float for property '" + std::string( token.value ) + "'." );
		return defaultValue;
	}
}

/**
*	@brief	Parses the string_value as an int32_t. If it fails, reports an error to the parser.
**/
int32_t shared_ast_node_t::GetPropertyInt( shared_parser_t& parser, const int32_t defaultValue ) const {
	if ( string_value.empty() ) {
		parser.ReportError( token, "Property '" + std::string( token.value ) + "' requires an integer value." );
		return defaultValue;
	}
	try {
		return std::stoi( string_value );
	} catch ( ... ) {
		parser.ReportError( token, "Failed to parse integer for property '" + std::string( token.value ) + "'." );
		return defaultValue;
	}
}

#endif // __cplusplus

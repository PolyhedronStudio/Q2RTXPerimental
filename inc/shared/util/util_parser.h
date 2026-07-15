/********************************************************************
*
*
*   Modular Parser / Tokenizer:
*
*
********************************************************************/
#pragma once

#ifdef __cplusplus

#include <string_view>
#include <vector>
#include <string>
#include <optional>

/**
*	@brief	Types of tokens emitted by the tokenizer.
**/
enum class shared_token_type_e {
	TOKEN_NONE,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_STRING,
	TOKEN_PUNCTUATION,
	TOKEN_NEWLINE,
	TOKEN_EOF,
	TOKEN_ERROR
};

/**
*	@brief	A single token from the source text.
**/
struct shared_token_t {
	shared_token_type_e type = shared_token_type_e::TOKEN_NONE;
	std::string_view value;
	int32_t line = 0;
	int32_t column = 0;
};

/**
*	@brief	Syntax modes for parser generic logic behavior.
**/
enum class shared_parser_mode_e {
	MODE_GENERIC,
	MODE_CSS,
	MODE_Q3_SHADER,
	MODE_Q2RTX_MATERIALS
};

/**
*	@brief	Tokenizer class that scans a string_view and yields tokens.
**/
class shared_tokenizer_t {
public:
	shared_tokenizer_t( const std::string_view source, const shared_parser_mode_e mode = shared_parser_mode_e::MODE_GENERIC );
	~shared_tokenizer_t() = default;

	/**
	*	@brief	Get the next token from the stream.
	**/
	shared_token_t NextToken();

	/**
	*	@brief	Peek at the next token without advancing.
	**/
	shared_token_t PeekToken();

	/**
	*	@brief	Get reported errors.
	**/
	const std::vector<std::string>& GetErrors() const { return m_errors; }
	bool HasErrors() const { return !m_errors.empty(); }

	void ReportError( const std::string& message );
	void ReportError( int32_t line, int32_t col, const std::string& message );

private:
	void SkipWhitespaceAndComments();
	shared_token_t ReadString();
	shared_token_t ReadNumber();
	shared_token_t ReadIdentifier();
	void Advance( size_t count = 1 );
	char PeekChar( size_t offset = 0 ) const;
	bool IsAtEnd() const;

	std::string_view m_source;
	shared_parser_mode_e m_mode;
	size_t m_position = 0;
	int32_t m_currentLine = 1;
	int32_t m_currentColumn = 1;

	std::optional<shared_token_t> m_peekedToken;
	std::vector<std::string> m_errors;
};

/**
*	@brief	Types of AST nodes.
**/
enum class shared_ast_node_type_e {
	NODE_ROOT,
	NODE_BLOCK,
	NODE_PROPERTY,
	NODE_TOKEN
};

class shared_parser_t;

/**
*	@brief	Abstract Syntax Tree node representing either a parsed token or a block scope.
**/
struct shared_ast_node_t {
	shared_ast_node_type_e type = shared_ast_node_type_e::NODE_TOKEN;
	shared_token_t token; // Valid if type == NODE_TOKEN or NODE_PROPERTY
	std::string string_value; // Valid if type == NODE_PROPERTY
	std::vector<shared_ast_node_t> children; // Valid if type == NODE_BLOCK or NODE_ROOT

	bool IsToken() const { return type == shared_ast_node_type_e::NODE_TOKEN; }
	bool IsBlock() const { return type == shared_ast_node_type_e::NODE_BLOCK; }
	bool IsProperty() const { return type == shared_ast_node_type_e::NODE_PROPERTY; }

	/**
	*	@brief	Returns true if this node is a property with the given name.
	**/
	bool IsProperty( const std::string_view expectedName ) const {
		return type == shared_ast_node_type_e::NODE_PROPERTY && token.value == expectedName;
	}

	/**
	*	@brief	Parses the string_value as a float. If it fails, reports an error to the parser.
	**/
	float GetPropertyFloat( shared_parser_t& parser, const float defaultValue = 0.0f ) const;

	/**
	*	@brief	Parses the string_value as an int32_t. If it fails, reports an error to the parser.
	**/
	int32_t GetPropertyInt( shared_parser_t& parser, const int32_t defaultValue = 0 ) const;

	/**
	*	@brief	Returns the child at `index` if it is a token, otherwise nullptr.
	**/
	const shared_ast_node_t* GetTokenChild( size_t index ) const {
		if ( index < children.size() && children[ index ].IsToken() ) {
			return &children[ index ];
		}
		return nullptr;
	}

	/**
	*	@brief	Returns the child at `index` if it is a block, otherwise nullptr.
	**/
	const shared_ast_node_t* GetBlockChild( size_t index ) const {
		if ( index < children.size() && children[ index ].IsBlock() ) {
			return &children[ index ];
		}
		return nullptr;
	}
};

/**
*	@brief	Parser that builds an AST from a tokenizer stream.
**/
class shared_parser_t {
public:
	shared_parser_t( const std::string_view source, const shared_parser_mode_e mode = shared_parser_mode_e::MODE_GENERIC );
	~shared_parser_t() = default;

	/**
	*	@brief	Parses the entire source and returns the root AST node.
	**/
	shared_ast_node_t Parse();

	const std::vector<std::string>& GetErrors() const { return m_tokenizer.GetErrors(); }
	bool HasErrors() const { return m_tokenizer.HasErrors(); }
	void ReportError( const shared_token_t& token, const std::string& message );

private:
	shared_ast_node_t ParseBlock();

	shared_tokenizer_t m_tokenizer;
	shared_parser_mode_e m_mode;
};

#endif // __cplusplus

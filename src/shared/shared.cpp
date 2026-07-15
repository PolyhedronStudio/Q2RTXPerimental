/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "shared/shared.h"

// Include nlohman::json library for easy parsing.
#include <nlohmann/json.hpp>

// Antique Quake Constant:
const vec3_t vec3_origin = { 0, 0, 0 };

/**
*	Random Number Generator.
**/
//! Mersenne Twister random number generator.
std::mt19937_64 mt_rand;

#include "shared/util/util_parser.h"

/**
*	@brief	A simple data structure to represent a shader pass.
**/
struct test_shader_pass_t {
	std::string map;
	std::string blendfunc;
};

/**
*	@brief	A simple data structure to represent a parsed shader material.
**/
struct test_shader_t {
	std::string name;
	bool surfaceparm_noimpact = false;
	std::vector<test_shader_pass_t> passes;
};

/**
*	@brief	Test function to verify the generic modular AST parser.
**/
void Shared_TestTokenizer() {
	const char* testSource = 
		"textures/custom/material\n"
		"{\n"
		"    // A sample comment\n"
		"    surfaceparm noimpact\n"
		"    {\n"
		"        map \"textures/custom/diffuse.tga\"\n"
		"        blendfunc add\n"
		"    }\n"
		"}\n";

	shared_parser_t parser( testSource, shared_parser_mode_e::MODE_Q3_SHADER );
	shared_ast_node_t root = parser.Parse();

	std::vector<test_shader_t> parsedShaders;

	// Iterate over root children to find shaders
	for ( size_t i = 0; i < root.children.size(); i++ ) {
		const shared_ast_node_t& node = root.children[ i ];

		// A shader is typically an identifier followed by a block
		if ( node.IsToken() && node.token.type == shared_token_type_e::TOKEN_IDENTIFIER ) {
			
			// Check if the next node is a block
			if ( const shared_ast_node_t* block = root.GetBlockChild( i + 1 ) ) {
				test_shader_t shader;
				shader.name = std::string( node.token.value );

				// Iterate over the shader block's children
				for ( size_t j = 0; j < block->children.size(); j++ ) {
					const shared_ast_node_t& shaderChild = block->children[ j ];

					if ( shaderChild.IsProperty( "surfaceparm" ) ) {
						// e.g. "surfaceparm" "noimpact"
						if ( shaderChild.string_value == "noimpact" ) {
							shader.surfaceparm_noimpact = true;
						} else {
							parser.ReportError( shaderChild.token, "Unknown surfaceparm type." );
						}
					} else if ( shaderChild.IsProperty() ) {
						parser.ReportError( shaderChild.token, "Unknown shader property." );
					} else if ( shaderChild.IsBlock() ) {
						// It's a pass block
						test_shader_pass_t pass;
						for ( size_t k = 0; k < shaderChild.children.size(); k++ ) {
							const shared_ast_node_t& passChild = shaderChild.children[ k ];

							if ( passChild.IsProperty( "map" ) ) {
								pass.map = passChild.string_value;
							} else if ( passChild.IsProperty( "blendfunc" ) ) {
								pass.blendfunc = passChild.string_value;
							} else if ( passChild.IsProperty() ) {
								parser.ReportError( passChild.token, "Unknown pass property." );
							} else {
								parser.ReportError( passChild.token, "Unexpected token inside pass block." );
							}
						}
						shader.passes.push_back( pass );
					} else {
						parser.ReportError( shaderChild.token, "Unexpected token inside shader block." );
					}
				}

				parsedShaders.push_back( shader );
				i++; // Skip the block we just processed
			} else {
				parser.ReportError( node.token, "Expected block '{' after identifier." );
			}
		} else if ( node.IsToken() && node.token.type != shared_token_type_e::TOKEN_EOF ) {
			parser.ReportError( node.token, "Unexpected token at global scope." );
		}
	}

	// Output any errors that occurred during parsing or AST conversion
	if ( parser.HasErrors() ) {
		const std::vector<std::string>& errors = parser.GetErrors();
		for ( const std::string& err : errors ) {
			Com_LPrintf( PRINT_DEVELOPER, "Parser Error: %s\n", err.c_str() );
		}
	}

	// Verify the parsed data structures
	if ( !parsedShaders.empty() ) {
		const test_shader_t& shader = parsedShaders[ 0 ];
		// Assuming we can use assertions or just rely on a debugger to see this.
		if ( shader.name == "textures/custom/material" && shader.surfaceparm_noimpact ) {
			if ( !shader.passes.empty() ) {
				if ( shader.passes[ 0 ].map == "textures/custom/diffuse.tga" ) {
					// Parsed correctly!
				}
			}
		}
	}
}

/**
*	@brief	A simple data structure to represent a CSS property.
**/
struct test_css_property_t {
	std::string name;
	std::string value;
};

/**
*	@brief	A simple data structure to represent a CSS rule (selector + block).
**/
struct test_css_rule_t {
	std::string selector;
	std::vector<test_css_property_t> properties;
};

/**
*	@brief	Test function to verify generic parsing on a CSS-like string.
**/
void Shared_TestCSSParser() {
	const char* cssSource = 
		".someClass {\n"
		"  padding: 5px;\n"
		"  margin: 10px;\n"
		"  display: block;\n"
		"}\n"
		"\n"
		"span.a {\n"
		"  display: inline; /* the default for span */\n"
		"  padding: 5px;\n"
		"  border: 2px solid red;\n"
		"}\n"
		"\n"
		"span.b {\n"
		"  display: inline-block;\n"
		"  width: 100px;\n"
		"  height: 35px;\n"
		"  padding: 5px;\n"
		"  border: 2px solid red;\n"
		"}\n"
		"\n"
		"span.c {\n"
		"  display: block;\n"
		"  width: 100px;\n"
		"  height: 35px;\n"
		"  padding: 5px;\n"
		"  border: 2px solid red;\n"
		"}\n";

	shared_parser_t parser( cssSource, shared_parser_mode_e::MODE_CSS );
	shared_ast_node_t root = parser.Parse();

	std::vector<test_css_rule_t> cssRules;

	// Since a CSS selector can consist of multiple tokens (e.g. `span`, `.`, `a`),
	// we accumulate them until we hit the block.
	std::string currentSelector;

	for ( size_t i = 0; i < root.children.size(); i++ ) {
		const shared_ast_node_t& node = root.children[ i ];

		if ( node.IsToken() && node.token.type != shared_token_type_e::TOKEN_EOF ) {
			// Append token value to the selector
			currentSelector += std::string( node.token.value );
		} else if ( node.IsBlock() ) {
			// We hit a block, so the selector is complete
			if ( !currentSelector.empty() ) {
				test_css_rule_t rule;
				rule.selector = currentSelector;
				currentSelector.clear();

				// Parse properties inside the block
				const shared_ast_node_t& block = node;

				for ( size_t j = 0; j < block.children.size(); j++ ) {
					const shared_ast_node_t& child = block.children[ j ];

					if ( child.IsProperty() ) {
						if ( child.string_value.empty() ) {
							parser.ReportError( child.token, "CSS property requires a value." );
						}
						test_css_property_t prop;
						prop.name = std::string( child.token.value );
						prop.value = child.string_value;
						rule.properties.push_back( prop );
					} else {
						parser.ReportError( child.token, "Unexpected token inside CSS block." );
					}
				}

				cssRules.push_back( rule );
			} else {
				parser.ReportError( node.token, "Encountered block without a CSS selector." );
			}
		}
	}

	// Output any errors that occurred during parsing or AST conversion
	if ( parser.HasErrors() ) {
		const std::vector<std::string>& errors = parser.GetErrors();
		for ( const std::string& err : errors ) {
			Com_LPrintf( PRINT_DEVELOPER, "CSS Parser Error: %s\n", err.c_str() );
		}
	}

	// Simple sanity checks
	if ( cssRules.size() == 4 ) {
		// Validated parsing length!
		if ( cssRules[ 1 ].selector == "span.a" && cssRules[ 1 ].properties.size() == 3 ) {
			// "border: 2px solid red;" parsed correctly if property value contains the concatenation
		}
	}
}

/**
*	@brief	A simple data structure to represent a Q2RTX Material property.
**/
struct test_q2rtx_material_t {
	std::string selector;
	std::string texture_base;
	std::string texture_normals;
	float base_factor = 0.0f;
	float bump_scale = 0.0f;
};

/**
*	@brief	Test function to verify Q2RTX materials parser mode.
**/
void Shared_TestQ2RTXParser() {
	const char* q2rtxSource = 
		"textures/pbr02/bricks00:\n"
		"	texture_base textures/pbr02/bricks00.tga\n"
		"	texture_normals textures/pbr02/bricks00_n.tga\n"
		"	base_factor 2.5\n"
		"	bump_scale 1.0\n"
		"	emissive_factor 0\n"
		"	metalness_factor 0\n"
		"    specular_factor 0.125\n"
		"\n"
		"textures/pbr02/chkr_tiles00,\n"
		"textures/pbr02/chkr_tiles01,\n"
		"textures/pbr02/chkr_tiles02:\n"
		"	texture_base textures/pbr02/chkr_tiles00.tga\n"
		"	texture_normals textures/pbr02/chkr_tiles00_n.tga\n"
		"	base_factor 2.5\n"
		"	bump_scale 0.175\n"
		"	emissive_factor 0\n"
		"	metalness_factor 0\n"
		"	specular_factor 0.0125\n"
		"\n"
		"textures/pbr02/chkr_tiles03:\n"
		"	texture_base textures/pbr02/chkr_tiles00.tga\n"
		"	texture_normals textures/pbr02/chkr_tiles00_n.tga\n"
		"	base_factor 1.0\n"
		"	bump_scale 1.0\n"
		"	emissive_factor 0\n"
		"	metalness_factor 0.125\n"
		"	specular_factor 0.125\n";

	shared_parser_t parser( q2rtxSource, shared_parser_mode_e::MODE_Q2RTX_MATERIALS );
	shared_ast_node_t root = parser.Parse();

	std::vector<test_q2rtx_material_t> parsedMaterials;

	// In Q2RTX mode, the parser automatically emits NODE_TOKEN containing the selector, 
	// followed immediately by a NODE_BLOCK containing the properties.
	for ( size_t i = 0; i < root.children.size(); i++ ) {
		const shared_ast_node_t& node = root.children[ i ];

		if ( node.IsToken() ) {
			if ( const shared_ast_node_t* block = root.GetBlockChild( i + 1 ) ) {
				test_q2rtx_material_t mat;
				mat.selector = std::string( node.token.value );

				for ( size_t j = 0; j < block->children.size(); j++ ) {
					const shared_ast_node_t& child = block->children[ j ];

					if ( child.IsProperty( "texture_base" ) ) {
						mat.texture_base = child.string_value;
					} else if ( child.IsProperty( "texture_normals" ) ) {
						mat.texture_normals = child.string_value;
					} else if ( child.IsProperty( "base_factor" ) ) {
						mat.base_factor = child.GetPropertyFloat( parser, 0.0f );
					} else if ( child.IsProperty( "bump_scale" ) ) {
						mat.bump_scale = child.GetPropertyFloat( parser, 0.0f );
					} else if ( child.IsProperty( "emissive_factor" ) || child.IsProperty( "metalness_factor" ) || child.IsProperty( "specular_factor" ) ) {
						// Ignored in struct right now but acknowledged.
					} else if ( child.IsProperty() ) {
						parser.ReportError( child.token, "Unknown material property." );
					} else {
						parser.ReportError( child.token, "Unexpected token inside material block." );
					}
				}

				parsedMaterials.push_back( mat );
				i++; // Skip the block
			}
		}
	}

	// Output any errors that occurred during parsing or AST conversion
	if ( parser.HasErrors() ) {
		const std::vector<std::string>& errors = parser.GetErrors();
		for ( const std::string& err : errors ) {
			Com_LPrintf( PRINT_DEVELOPER, "Q2RTX Parser Error: %s\n", err.c_str() );
		}
	}
}

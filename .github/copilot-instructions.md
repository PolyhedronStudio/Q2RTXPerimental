# Copilot Instructions

## General Guidelines
- Use 1TBS/K&R braces everywhere for new/modified code.
- Always use `nullptr` (avoid `NULL`).
- Prefer tabs for indentation; otherwise, use 4 spaces unless the file predominantly uses a different indent width. Match the immediate file’s indentation quirks.

## Code Style
- Follow consistent formatting rules across the project.
- Adhere to naming conventions as specified in the project documentation.
- Prefer Doxygen-style comments with tab-indented @tags (e.g., /**\n*\t@brief ...\n*\t@note ...\n**/) and section header blocks in .cpp files.
- Use explanatory // comments before small code blocks.
- Use spaced parameter style in function declarations, e.g., `void Func( const Type &param, const int32_t x, const bool flag = false );`
- For header (or rare TU) class declarations, use `svg_func_plat_trigger_t` as the canonical minimal example: sectioned Doxygen blocks for Construct/Destruct, DefineWorldSpawnClass, Save Descriptor Fields, Core (Reset/Save/Restore), Callback Member Functions, Member Functions, Member Variables; with small //! member comments and the established indentation/spacing.
# Per-tool implementation source lists for the compiler.
#
# Add dev/src/foo.cpp to the tools that use it by adding `foo` below. For
# subdirectories, use the path without `.cpp`, such as `parser/foo`.

FRONTEND_SOURCE_SET_TARGETS := abimangle pptoken posttoken ctrlexpr macro preproc recog nsdecl nsinit cy86 cppgm++ lowiropt lowir2cy86 lowir2native

FRONTEND_OBJ_BASENAMES_abimangle :=
FRONTEND_OBJ_BASENAMES_pptoken := pptoken_translation
FRONTEND_OBJ_BASENAMES_posttoken := pptoken_translation posttoken_unicode posttoken_lexer posttoken_semantics
FRONTEND_OBJ_BASENAMES_ctrlexpr := pptoken_translation posttoken_unicode ctrlexpr
FRONTEND_OBJ_BASENAMES_macro := macro_engine posttoken_lexer posttoken_unicode posttoken_semantics pptoken_translation
FRONTEND_OBJ_BASENAMES_preproc := preprocessor_engine macro_engine posttoken_lexer posttoken_unicode posttoken_semantics pptoken_translation ctrlexpr
FRONTEND_OBJ_BASENAMES_recog := recog_parser recog_parser_expressions recog_parser_declarations recog_parser_statements preprocessor_engine macro_engine posttoken_lexer posttoken_unicode posttoken_semantics pptoken_translation ctrlexpr
FRONTEND_OBJ_BASENAMES_nsdecl := nsdecl_parser preprocessor_engine macro_engine posttoken_lexer posttoken_unicode posttoken_semantics pptoken_translation ctrlexpr
FRONTEND_OBJ_BASENAMES_nsinit := nsinit_parser nsinit_model nsinit_literals nsinit_image preprocessor_engine macro_engine posttoken_lexer posttoken_unicode posttoken_semantics pptoken_translation ctrlexpr
FRONTEND_OBJ_BASENAMES_cy86 :=
FRONTEND_OBJ_BASENAMES_cppgm++ :=
FRONTEND_OBJ_BASENAMES_lowiropt :=
FRONTEND_OBJ_BASENAMES_lowir2cy86 :=
FRONTEND_OBJ_BASENAMES_lowir2native :=

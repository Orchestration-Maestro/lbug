# Copilot instructions for lbug, patched for Maestro

## Start here

This repository carries [`lbug`](https://crates.io/crates/lbug), the Rust
binding of [LadybugDB](https://github.com/LadybugDB/ladybug), with one small
patch that Maestro needs. The code is upstream's, under its MIT licence
(`LICENSE`); bundled third-party sources keep their own licences under
`lbug-src/third_party`.

Paths below are relative to this repository. Before editing, read
[AGENTS.md](../AGENTS.md) for the rules that bind every change and
[CONTRIBUTING.md](https://github.com/Orchestration-Maestro/.github/blob/main/CONTRIBUTING.md)
for how a change is proposed. The organization's [golden
rules](https://github.com/Orchestration-Maestro/.github/blob/main/golden-rules/engineering.md)
come first: nothing in a specification, a plan or this repository weakens them.

For quality, engineering or security changes, read
[northstar.md](../docs/standards/northstar.md),
[engineering.md](../docs/standards/engineering.md) and
[security.md](../docs/standards/security.md): this repository's map of the
organization's golden rules.

Keep changes scoped to the request, and read historical plans and specifications
as records, not as instructions to start new work.

## Repository tree

Every tracked file, with what it is for. `rust-gate guide` writes this tree at
every commit and keeps each explanation already here, so improve an explanation
in place.

```text
.                                                                # Repository root
├── .github/                                                     # GitHub metadata, templates and workflows
│   └── copilot-instructions.md                                  # This guide, written by rust-gate guide at every commit
├── docs/                                                        # Documentation
│   └── standards/                                               # Standards
│       ├── engineering.md                                       # Engineering rules in lbug
│       ├── northstar.md                                         # Northstar for lbug
│       └── security.md                                          # Security rules in lbug
├── include/                                                     # Include
│   ├── lbug_arrow.h                                             # File: lbug arrow
│   └── lbug_rs.h                                                # File: lbug rs
├── lbug-src/                                                    # Ladybug is being developed by LadybugDB Developers and is available under a permissive license
│   ├── cmake/                                                   # Cmake
│   │   ├── templates/                                           # Templates
│   │   │   └── system_config.h.in                               # File: system config.h
│   │   └── BundleStaticLibrary.cmake                            # File: BundleStaticLibrary
│   ├── src/                                                     # The crate's sources
│   │   ├── antlr4/                                              # Neither Cypher.g4 nor keywords.txt can be individually used to generate Ladybug's grammar
│   │   │   ├── Cypher.g4                                        # File: Cypher
│   │   │   ├── README.md                                        # Neither Cypher.g4 nor keywords.txt can be individually used to generate Ladybug's grammar
│   │   │   └── keywords.txt                                     # Text: keywords
│   │   ├── binder/                                              # Binder
│   │   │   ├── bind/                                            # Bind
│   │   │   │   ├── copy/                                        # Copy
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── bind_copy_from.cpp                       # File: bind copy from
│   │   │   │   │   └── bind_copy_to.cpp                         # File: bind copy to
│   │   │   │   ├── ddl/                                         # Ddl
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   └── bound_create_table_info.cpp              # File: bound create table info
│   │   │   │   ├── read/                                        # Read
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── bind_in_query_call.cpp                   # File: bind in query call
│   │   │   │   │   ├── bind_load_from.cpp                       # File: bind load from
│   │   │   │   │   ├── bind_match.cpp                           # File: bind match
│   │   │   │   │   └── bind_unwind.cpp                          # File: bind unwind
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bind_analyze.cpp                             # File: bind analyze
│   │   │   │   ├── bind_attach_database.cpp                     # File: bind attach database
│   │   │   │   ├── bind_create_macro.cpp                        # File: bind create macro
│   │   │   │   ├── bind_ddl.cpp                                 # File: bind ddl
│   │   │   │   ├── bind_detach_database.cpp                     # File: bind detach database
│   │   │   │   ├── bind_explain.cpp                             # File: bind explain
│   │   │   │   ├── bind_export_database.cpp                     # File: bind export database
│   │   │   │   ├── bind_extension.cpp                           # File: bind extension
│   │   │   │   ├── bind_extension_clause.cpp                    # File: bind extension clause
│   │   │   │   ├── bind_file_scan.cpp                           # File: bind file scan
│   │   │   │   ├── bind_graph.cpp                               # File: bind graph
│   │   │   │   ├── bind_graph_pattern.cpp                       # File: bind graph pattern
│   │   │   │   ├── bind_import_database.cpp                     # File: bind import database
│   │   │   │   ├── bind_projection_clause.cpp                   # File: bind projection clause
│   │   │   │   ├── bind_query.cpp                               # File: bind query
│   │   │   │   ├── bind_reading_clause.cpp                      # File: bind reading clause
│   │   │   │   ├── bind_standalone_call.cpp                     # File: bind standalone call
│   │   │   │   ├── bind_standalone_call_function.cpp            # File: bind standalone call function
│   │   │   │   ├── bind_table_function.cpp                      # File: bind table function
│   │   │   │   ├── bind_transaction.cpp                         # File: bind transaction
│   │   │   │   ├── bind_updating_clause.cpp                     # File: bind updating clause
│   │   │   │   └── bind_use_database.cpp                        # File: bind use database
│   │   │   ├── bind_expression/                                 # Bind expression
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bind_boolean_expression.cpp                  # File: bind boolean expression
│   │   │   │   ├── bind_case_expression.cpp                     # File: bind case expression
│   │   │   │   ├── bind_comparison_expression.cpp               # File: bind comparison expression
│   │   │   │   ├── bind_function_expression.cpp                 # File: bind function expression
│   │   │   │   ├── bind_lambda_expression.cpp                   # File: bind lambda expression
│   │   │   │   ├── bind_literal_expression.cpp                  # File: bind literal expression
│   │   │   │   ├── bind_null_operator_expression.cpp            # File: bind null operator expression
│   │   │   │   ├── bind_parameter_expression.cpp                # File: bind parameter expression
│   │   │   │   ├── bind_property_expression.cpp                 # File: bind property expression
│   │   │   │   ├── bind_subquery_expression.cpp                 # File: bind subquery expression
│   │   │   │   └── bind_variable_expression.cpp                 # File: bind variable expression
│   │   │   ├── ddl/                                             # Ddl
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bound_alter_info.cpp                         # File: bound alter info
│   │   │   │   └── property_definition.cpp                      # File: property definition
│   │   │   ├── expression/                                      # Expression
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── aggregate_function_expression.cpp            # File: aggregate function expression
│   │   │   │   ├── case_expression.cpp                          # File: case expression
│   │   │   │   ├── expression.cpp                               # File: expression
│   │   │   │   ├── expression_util.cpp                          # File: expression util
│   │   │   │   ├── literal_expression.cpp                       # File: literal expression
│   │   │   │   ├── node_expression.cpp                          # File: node expression
│   │   │   │   ├── node_rel_expression.cpp                      # File: node rel expression
│   │   │   │   ├── parameter_expression.cpp                     # File: parameter expression
│   │   │   │   ├── property_expression.cpp                      # File: property expression
│   │   │   │   ├── rel_expression.cpp                           # File: rel expression
│   │   │   │   ├── scalar_function_expression.cpp               # File: scalar function expression
│   │   │   │   └── variable_expression.cpp                      # File: variable expression
│   │   │   ├── query/                                           # Query
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bound_delete_clause.cpp                      # File: bound delete clause
│   │   │   │   ├── bound_insert_clause.cpp                      # File: bound insert clause
│   │   │   │   ├── bound_merge_clause.cpp                       # File: bound merge clause
│   │   │   │   ├── bound_set_clause.cpp                         # File: bound set clause
│   │   │   │   ├── query_graph.cpp                              # File: query graph
│   │   │   │   └── query_graph_label_analyzer.cpp               # File: query graph label analyzer
│   │   │   ├── rewriter/                                        # Rewriter
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── match_clause_pattern_label_rewriter.cpp      # File: match clause pattern label rewriter
│   │   │   │   ├── normalized_query_part_match_rewriter.cpp     # File: normalized query part match rewriter
│   │   │   │   └── with_clause_projection_rewriter.cpp          # File: with clause projection rewriter
│   │   │   ├── visitor/                                         # Visitor
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── confidential_statement_analyzer.cpp          # File: confidential statement analyzer
│   │   │   │   ├── default_type_solver.cpp                      # File: default type solver
│   │   │   │   └── property_collector.cpp                       # File: property collector
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── binder.cpp                                       # File: binder
│   │   │   ├── binder_scope.cpp                                 # File: binder scope
│   │   │   ├── bound_scan_source.cpp                            # File: bound scan source
│   │   │   ├── bound_statement_result.cpp                       # File: bound statement result
│   │   │   ├── bound_statement_rewriter.cpp                     # File: bound statement rewriter
│   │   │   ├── bound_statement_visitor.cpp                      # File: bound statement visitor
│   │   │   ├── expression_binder.cpp                            # File: expression binder
│   │   │   └── expression_visitor.cpp                           # File: expression visitor
│   │   ├── c_api/                                               # C api
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── connection.cpp                                   # File: connection
│   │   │   ├── data_type.cpp                                    # File: data type
│   │   │   ├── database.cpp                                     # File: database
│   │   │   ├── flat_tuple.cpp                                   # File: flat tuple
│   │   │   ├── helpers.cpp                                      # File: helpers
│   │   │   ├── prepared_statement.cpp                           # File: prepared statement
│   │   │   ├── query_result.cpp                                 # File: query result
│   │   │   ├── query_summary.cpp                                # File: query summary
│   │   │   ├── value.cpp                                        # File: value
│   │   │   └── version.cpp                                      # File: version
│   │   ├── catalog/                                             # Catalog
│   │   │   ├── catalog_entry/                                   # Catalog entry
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── catalog_entry.cpp                            # File: catalog entry
│   │   │   │   ├── catalog_entry_type.cpp                       # File: catalog entry type
│   │   │   │   ├── function_catalog_entry.cpp                   # File: function catalog entry
│   │   │   │   ├── graph_catalog_entry.cpp                      # File: graph catalog entry
│   │   │   │   ├── index_catalog_entry.cpp                      # File: index catalog entry
│   │   │   │   ├── node_table_catalog_entry.cpp                 # File: node table catalog entry
│   │   │   │   ├── node_table_id_pair.cpp                       # File: node table id pair
│   │   │   │   ├── rel_group_catalog_entry.cpp                  # File: rel group catalog entry
│   │   │   │   ├── scalar_macro_catalog_entry.cpp               # File: scalar macro catalog entry
│   │   │   │   ├── sequence_catalog_entry.cpp                   # File: sequence catalog entry
│   │   │   │   ├── table_catalog_entry.cpp                      # File: table catalog entry
│   │   │   │   └── type_catalog_entry.cpp                       # File: type catalog entry
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── catalog.cpp                                      # File: catalog
│   │   │   ├── catalog_set.cpp                                  # File: catalog set
│   │   │   └── property_definition_collection.cpp               # File: property definition collection
│   │   ├── common/                                              # Common
│   │   │   ├── arrow/                                           # Arrow
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── arrow_array_scan.cpp                         # File: arrow array scan
│   │   │   │   ├── arrow_converter.cpp                          # File: arrow converter
│   │   │   │   ├── arrow_null_mask_tree.cpp                     # File: arrow null mask tree
│   │   │   │   ├── arrow_row_batch.cpp                          # File: arrow row batch
│   │   │   │   ├── arrow_schema_metadata.cpp                    # File: arrow schema metadata
│   │   │   │   ├── arrow_schema_metadata_generic_decoder.cpp    # File: arrow schema metadata generic decoder
│   │   │   │   ├── arrow_schema_metadata_internal.h             # File: arrow schema metadata internal
│   │   │   │   ├── arrow_schema_metadata_snowflake_decoder.cpp  # File: arrow schema metadata snowflake decoder
│   │   │   │   ├── arrow_schema_metadata_utils.cpp              # File: arrow schema metadata utils
│   │   │   │   └── arrow_type.cpp                               # File: arrow type
│   │   │   ├── copier_config/                                   # Copier config
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── csv_reader_config.cpp                        # File: csv reader config
│   │   │   │   └── reader_config.cpp                            # File: reader config
│   │   │   ├── data_chunk/                                      # Data chunk
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── data_chunk.cpp                               # File: data chunk
│   │   │   │   ├── data_chunk_collection.cpp                    # File: data chunk collection
│   │   │   │   ├── data_chunk_state.cpp                         # File: data chunk state
│   │   │   │   └── sel_vector.cpp                               # File: sel vector
│   │   │   ├── enums/                                           # Enums
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── accumulate_type.cpp                          # File: accumulate type
│   │   │   │   ├── conflict_action.cpp                          # File: conflict action
│   │   │   │   ├── drop_type.cpp                                # File: drop type
│   │   │   │   ├── extend_direction_util.cpp                    # File: extend direction util
│   │   │   │   ├── path_semantic.cpp                            # File: path semantic
│   │   │   │   ├── query_rel_type.cpp                           # File: query rel type
│   │   │   │   ├── rel_direction.cpp                            # File: rel direction
│   │   │   │   ├── rel_multiplicity.cpp                         # File: rel multiplicity
│   │   │   │   ├── scan_source_type.cpp                         # File: scan source type
│   │   │   │   ├── storage_format.cpp                           # File: storage format
│   │   │   │   ├── table_type.cpp                               # File: table type
│   │   │   │   └── transaction_action.cpp                       # File: transaction action
│   │   │   ├── exception/                                       # Exception
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── exception.cpp                                # File: exception
│   │   │   │   └── message.cpp                                  # File: message
│   │   │   ├── file_system/                                     # File system
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── compressed_file_system.cpp                   # File: compressed file system
│   │   │   │   ├── file_info.cpp                                # File: file info
│   │   │   │   ├── file_system.cpp                              # File: file system
│   │   │   │   ├── gzip_file_system.cpp                         # File: gzip file system
│   │   │   │   ├── local_file_system.cpp                        # File: local file system
│   │   │   │   └── virtual_file_system.cpp                      # File: virtual file system
│   │   │   ├── serializer/                                      # Serializer
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── buffer_writer.cpp                            # File: buffer writer
│   │   │   │   ├── buffered_file.cpp                            # File: buffered file
│   │   │   │   ├── deserializer.cpp                             # File: deserializer
│   │   │   │   ├── in_mem_file_writer.cpp                       # File: in mem file writer
│   │   │   │   └── serializer.cpp                               # File: serializer
│   │   │   ├── signal/                                          # Signal
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── register.cpp                                 # File: register
│   │   │   ├── task_system/                                     # Task system
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── progress_bar.cpp                             # File: progress bar
│   │   │   │   ├── task.cpp                                     # File: task
│   │   │   │   ├── task_scheduler.cpp                           # File: task scheduler
│   │   │   │   └── terminal_progress_bar_display.cpp            # File: terminal progress bar display
│   │   │   ├── types/                                           # Types
│   │   │   │   ├── value/                                       # Value
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── nested.cpp                               # File: nested
│   │   │   │   │   ├── node.cpp                                 # File: node
│   │   │   │   │   ├── recursive_rel.cpp                        # File: recursive rel
│   │   │   │   │   ├── rel.cpp                                  # File: rel
│   │   │   │   │   └── value.cpp                                # File: value
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── blob.cpp                                     # File: blob
│   │   │   │   ├── date_t.cpp                                   # File: date t
│   │   │   │   ├── dtime_t.cpp                                  # File: dtime t
│   │   │   │   ├── int128_t.cpp                                 # File: int128 t
│   │   │   │   ├── interval_t.cpp                               # File: interval t
│   │   │   │   ├── json_type.cpp                                # File: json type
│   │   │   │   ├── list_t.cpp                                   # File: list t
│   │   │   │   ├── string_t.cpp                                 # File: string t
│   │   │   │   ├── timestamp_t.cpp                              # File: timestamp t
│   │   │   │   ├── types.cpp                                    # File: types
│   │   │   │   ├── uint128_t.cpp                                # File: uint128 t
│   │   │   │   └── uuid.cpp                                     # File: uuid
│   │   │   ├── vector/                                          # Vector
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── auxiliary_buffer.cpp                         # File: auxiliary buffer
│   │   │   │   └── value_vector.cpp                             # File: value vector
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── case_insensitive_map.cpp                         # File: case insensitive map
│   │   │   ├── checksum.cpp                                     # File: checksum
│   │   │   ├── constants.cpp                                    # File: constants
│   │   │   ├── database_lifecycle_manager.cpp                   # File: database lifecycle manager
│   │   │   ├── expression_type.cpp                              # File: expression type
│   │   │   ├── in_mem_overflow_buffer.cpp                       # File: in mem overflow buffer
│   │   │   ├── json_utils.cpp                                   # File: json utils
│   │   │   ├── mask.cpp                                         # File: mask
│   │   │   ├── md5.cpp                                          # File: md5
│   │   │   ├── metric.cpp                                       # File: metric
│   │   │   ├── null_mask.cpp                                    # File: null mask
│   │   │   ├── partition_routing_hook.cpp                       # File: partition routing hook
│   │   │   ├── profiler.cpp                                     # File: profiler
│   │   │   ├── random_engine.cpp                                # File: random engine
│   │   │   ├── roaring_mask.cpp                                 # File: roaring mask
│   │   │   ├── sha256.cpp                                       # File: sha256
│   │   │   ├── string_utils.cpp                                 # File: string utils
│   │   │   ├── system_message.cpp                               # File: system message
│   │   │   ├── type_utils.cpp                                   # File: type utils
│   │   │   ├── utils.cpp                                        # File: utils
│   │   │   └── windows_utils.cpp                                # File: windows utils
│   │   ├── expression_evaluator/                                # Expression evaluator
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── case_evaluator.cpp                               # File: case evaluator
│   │   │   ├── expression_evaluator.cpp                         # File: expression evaluator
│   │   │   ├── expression_evaluator_utils.cpp                   # File: expression evaluator utils
│   │   │   ├── expression_evaluator_visitor.cpp                 # File: expression evaluator visitor
│   │   │   ├── function_evaluator.cpp                           # File: function evaluator
│   │   │   ├── lambda_evaluator.cpp                             # File: lambda evaluator
│   │   │   ├── list_slice_info.cpp                              # File: list slice info
│   │   │   ├── literal_evaluator.cpp                            # File: literal evaluator
│   │   │   ├── path_evaluator.cpp                               # File: path evaluator
│   │   │   ├── pattern_evaluator.cpp                            # File: pattern evaluator
│   │   │   └── reference_evaluator.cpp                          # File: reference evaluator
│   │   ├── extension/                                           # Extension
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── catalog_extension.cpp                            # File: catalog extension
│   │   │   ├── extension.cpp                                    # File: extension
│   │   │   ├── extension_entries.cpp                            # File: extension entries
│   │   │   ├── extension_installer.cpp                          # File: extension installer
│   │   │   ├── extension_manager.cpp                            # File: extension manager
│   │   │   ├── generated_extension_loader.cpp.in                # File: generated extension loader.cpp
│   │   │   ├── generated_extension_loader.h.in                  # File: generated extension loader.h
│   │   │   └── loaded_extension.cpp                             # File: loaded extension
│   │   ├── function/                                            # Function
│   │   │   ├── aggregate/                                       # Aggregate
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── avg.cpp                                      # File: avg
│   │   │   │   ├── collect.cpp                                  # File: collect
│   │   │   │   ├── count.cpp                                    # File: count
│   │   │   │   ├── count_star.cpp                               # File: count star
│   │   │   │   ├── histogram.cpp                                # File: histogram
│   │   │   │   ├── min_max.cpp                                  # File: min max
│   │   │   │   ├── percentile_cont.cpp                          # File: percentile cont
│   │   │   │   ├── percentile_disc.cpp                          # File: percentile disc
│   │   │   │   └── sum.cpp                                      # File: sum
│   │   │   ├── arithmetic/                                      # Arithmetic
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── abs.cpp                                      # File: abs
│   │   │   │   ├── add.cpp                                      # File: add
│   │   │   │   ├── divide.cpp                                   # File: divide
│   │   │   │   ├── modulo.cpp                                   # File: modulo
│   │   │   │   ├── multiply.cpp                                 # File: multiply
│   │   │   │   ├── negate.cpp                                   # File: negate
│   │   │   │   ├── rand_function.cpp                            # File: rand function
│   │   │   │   ├── set_seed.cpp                                 # File: set seed
│   │   │   │   └── subtract.cpp                                 # File: subtract
│   │   │   ├── array/                                           # Array
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── array_functions.cpp                          # File: array functions
│   │   │   │   └── array_value.cpp                              # File: array value
│   │   │   ├── cast/                                            # Cast
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── cast_array.cpp                               # File: cast array
│   │   │   ├── date/                                            # Date
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── date_functions.cpp                           # File: date functions
│   │   │   ├── export/                                          # Export
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── export_csv_function.cpp                      # File: export csv function
│   │   │   │   └── export_parquet_function.cpp                  # File: export parquet function
│   │   │   ├── gds/                                             # Gds
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── asp_destinations.cpp                         # File: asp destinations
│   │   │   │   ├── asp_paths.cpp                                # File: asp paths
│   │   │   │   ├── awsp_paths.cpp                               # File: awsp paths
│   │   │   │   ├── bfs_graph.cpp                                # File: bfs graph
│   │   │   │   ├── frontier_morsel.cpp                          # File: frontier morsel
│   │   │   │   ├── gds.cpp                                      # File: gds
│   │   │   │   ├── gds_frontier.cpp                             # File: gds frontier
│   │   │   │   ├── gds_state.cpp                                # File: gds state
│   │   │   │   ├── gds_task.cpp                                 # File: gds task
│   │   │   │   ├── gds_utils.cpp                                # File: gds utils
│   │   │   │   ├── output_writer.cpp                            # File: output writer
│   │   │   │   ├── rec_joins.cpp                                # File: rec joins
│   │   │   │   ├── ssp_destinations.cpp                         # File: ssp destinations
│   │   │   │   ├── ssp_paths.cpp                                # File: ssp paths
│   │   │   │   ├── variable_length_path.cpp                     # File: variable length path
│   │   │   │   ├── wsp_destinations.cpp                         # File: wsp destinations
│   │   │   │   └── wsp_paths.cpp                                # File: wsp paths
│   │   │   ├── internal_id/                                     # Internal id
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── internal_id_creation_function.cpp            # File: internal id creation function
│   │   │   ├── list/                                            # List
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── list_agg_function.cpp                        # File: list agg function
│   │   │   │   ├── list_all.cpp                                 # File: list all
│   │   │   │   ├── list_any.cpp                                 # File: list any
│   │   │   │   ├── list_any_value_function.cpp                  # File: list any value function
│   │   │   │   ├── list_append_function.cpp                     # File: list append function
│   │   │   │   ├── list_concat_function.cpp                     # File: list concat function
│   │   │   │   ├── list_contains_function.cpp                   # File: list contains function
│   │   │   │   ├── list_creation.cpp                            # File: list creation
│   │   │   │   ├── list_distinct_function.cpp                   # File: list distinct function
│   │   │   │   ├── list_extract_function.cpp                    # File: list extract function
│   │   │   │   ├── list_filter.cpp                              # File: list filter
│   │   │   │   ├── list_function_utils.cpp                      # File: list function utils
│   │   │   │   ├── list_has_all.cpp                             # File: list has all
│   │   │   │   ├── list_none.cpp                                # File: list none
│   │   │   │   ├── list_position_function.cpp                   # File: list position function
│   │   │   │   ├── list_prepend_function.cpp                    # File: list prepend function
│   │   │   │   ├── list_range_function.cpp                      # File: list range function
│   │   │   │   ├── list_reduce.cpp                              # File: list reduce
│   │   │   │   ├── list_reverse_function.cpp                    # File: list reverse function
│   │   │   │   ├── list_single.cpp                              # File: list single
│   │   │   │   ├── list_slice_function.cpp                      # File: list slice function
│   │   │   │   ├── list_sort_function.cpp                       # File: list sort function
│   │   │   │   ├── list_to_string_function.cpp                  # File: list to string function
│   │   │   │   ├── list_transform.cpp                           # File: list transform
│   │   │   │   ├── list_unique_function.cpp                     # File: list unique function
│   │   │   │   ├── quantifier_functions.cpp                     # File: quantifier functions
│   │   │   │   └── size_function.cpp                            # File: size function
│   │   │   ├── map/                                             # Map
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── map_creation_function.cpp                    # File: map creation function
│   │   │   │   ├── map_extract_function.cpp                     # File: map extract function
│   │   │   │   ├── map_keys_function.cpp                        # File: map keys function
│   │   │   │   └── map_values_function.cpp                      # File: map values function
│   │   │   ├── path/                                            # Path
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── length_function.cpp                          # File: length function
│   │   │   │   ├── nodes_function.cpp                           # File: nodes function
│   │   │   │   ├── properties_function.cpp                      # File: properties function
│   │   │   │   ├── rels_function.cpp                            # File: rels function
│   │   │   │   └── semantic_function.cpp                        # File: semantic function
│   │   │   ├── pattern/                                         # Pattern
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── cost_function.cpp                            # File: cost function
│   │   │   │   ├── id_function.cpp                              # File: id function
│   │   │   │   ├── label_function.cpp                           # File: label function
│   │   │   │   ├── rowid_function.cpp                           # File: rowid function
│   │   │   │   └── start_end_node_function.cpp                  # File: start end node function
│   │   │   ├── sequence/                                        # Sequence
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── sequence_functions.cpp                       # File: sequence functions
│   │   │   ├── string/                                          # String
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── concat_ws.cpp                                # File: concat ws
│   │   │   │   ├── init_cap_function.cpp                        # File: init cap function
│   │   │   │   ├── levenshtein_function.cpp                     # File: levenshtein function
│   │   │   │   ├── regex_full_match_function.cpp                # File: regex full match function
│   │   │   │   ├── regex_replace_function.cpp                   # File: regex replace function
│   │   │   │   ├── split_part.cpp                               # File: split part
│   │   │   │   └── string_split_function.cpp                    # File: string split function
│   │   │   ├── struct/                                          # Struct
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── keys_function.cpp                            # File: keys function
│   │   │   │   ├── struct_extract_function.cpp                  # File: struct extract function
│   │   │   │   └── struct_pack_function.cpp                     # File: struct pack function
│   │   │   ├── table/                                           # Table
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bind_data.cpp                                # File: bind data
│   │   │   │   ├── bind_input.cpp                               # File: bind input
│   │   │   │   ├── bm_info.cpp                                  # File: bm info
│   │   │   │   ├── cache_column.cpp                             # File: cache column
│   │   │   │   ├── catalog_version.cpp                          # File: catalog version
│   │   │   │   ├── clear_warnings.cpp                           # File: clear warnings
│   │   │   │   ├── current_setting.cpp                          # File: current setting
│   │   │   │   ├── db_version.cpp                               # File: db version
│   │   │   │   ├── disk_size_info.cpp                           # File: disk size info
│   │   │   │   ├── drop_project_graph.cpp                       # File: drop project graph
│   │   │   │   ├── file_info.cpp                                # File: file info
│   │   │   │   ├── free_space_info.cpp                          # File: free space info
│   │   │   │   ├── project_cypher_graph.cpp                     # File: project cypher graph
│   │   │   │   ├── project_native_graph.cpp                     # File: project native graph
│   │   │   │   ├── projected_graph_info.cpp                     # File: projected graph info
│   │   │   │   ├── show_attached_databases.cpp                  # File: show attached databases
│   │   │   │   ├── show_connection.cpp                          # File: show connection
│   │   │   │   ├── show_functions.cpp                           # File: show functions
│   │   │   │   ├── show_graphs.cpp                              # File: show graphs
│   │   │   │   ├── show_indexes.cpp                             # File: show indexes
│   │   │   │   ├── show_loaded_extensions.cpp                   # File: show loaded extensions
│   │   │   │   ├── show_macros.cpp                              # File: show macros
│   │   │   │   ├── show_official_extensions.cpp                 # File: show official extensions
│   │   │   │   ├── show_projected_graphs.cpp                    # File: show projected graphs
│   │   │   │   ├── show_sequences.cpp                           # File: show sequences
│   │   │   │   ├── show_tables.cpp                              # File: show tables
│   │   │   │   ├── show_warnings.cpp                            # File: show warnings
│   │   │   │   ├── simple_table_function.cpp                    # File: simple table function
│   │   │   │   ├── stats_info.cpp                               # File: stats info
│   │   │   │   ├── storage_info.cpp                             # File: storage info
│   │   │   │   ├── storage_version.cpp                          # File: storage version
│   │   │   │   ├── table_function.cpp                           # File: table function
│   │   │   │   └── table_info.cpp                               # File: table info
│   │   │   ├── timestamp/                                       # Timestamp
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── time_bucket.cpp                              # File: time bucket
│   │   │   │   └── to_epoch_ms.cpp                              # File: to epoch ms
│   │   │   ├── union/                                           # Union
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── union_extract_function.cpp                   # File: union extract function
│   │   │   │   ├── union_tag_function.cpp                       # File: union tag function
│   │   │   │   └── union_value_function.cpp                     # File: union value function
│   │   │   ├── utility/                                         # Utility
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── coalesce.cpp                                 # File: coalesce
│   │   │   │   ├── constant_or_null.cpp                         # File: constant or null
│   │   │   │   ├── count_if.cpp                                 # File: count if
│   │   │   │   ├── error.cpp                                    # File: error
│   │   │   │   ├── md5.cpp                                      # File: md5
│   │   │   │   ├── nullif.cpp                                   # File: nullif
│   │   │   │   ├── sha256.cpp                                   # File: sha256
│   │   │   │   └── typeof.cpp                                   # File: typeof
│   │   │   ├── uuid/                                            # Uuid
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   └── gen_random_uuid.cpp                          # File: gen random uuid
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── aggregate_function.cpp                           # File: aggregate function
│   │   │   ├── base_lower_upper_operation.cpp                   # File: base lower upper operation
│   │   │   ├── built_in_function_utils.cpp                      # File: built in function utils
│   │   │   ├── cast_from_string_functions.cpp                   # File: cast from string functions
│   │   │   ├── cast_string_non_nested_functions.cpp             # File: cast string non nested functions
│   │   │   ├── comparison_functions.cpp                         # File: comparison functions
│   │   │   ├── find_function.cpp                                # File: find function
│   │   │   ├── function.cpp                                     # File: function
│   │   │   ├── function_collection.cpp                          # File: function collection
│   │   │   ├── scalar_macro_function.cpp                        # File: scalar macro function
│   │   │   ├── simd_filter.cpp                                  # File: simd filter
│   │   │   ├── simd_filter_avx2.cpp                             # File: simd filter avx2
│   │   │   ├── simd_filter_neon.cpp                             # File: simd filter neon
│   │   │   ├── vector_arithmetic_functions.cpp                  # File: vector arithmetic functions
│   │   │   ├── vector_blob_functions.cpp                        # File: vector blob functions
│   │   │   ├── vector_boolean_functions.cpp                     # File: vector boolean functions
│   │   │   ├── vector_cast_functions.cpp                        # File: vector cast functions
│   │   │   ├── vector_date_functions.cpp                        # File: vector date functions
│   │   │   ├── vector_hash_functions.cpp                        # File: vector hash functions
│   │   │   ├── vector_node_rel_functions.cpp                    # File: vector node rel functions
│   │   │   ├── vector_null_functions.cpp                        # File: vector null functions
│   │   │   ├── vector_string_functions.cpp                      # File: vector string functions
│   │   │   ├── vector_timestamp_functions.cpp                   # File: vector timestamp functions
│   │   │   └── vector_uuid_functions.cpp                        # File: vector uuid functions
│   │   ├── graph/                                               # Graph
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── graph.cpp                                        # File: graph
│   │   │   ├── graph_entry.cpp                                  # File: graph entry
│   │   │   ├── graph_entry_set.cpp                              # File: graph entry set
│   │   │   ├── on_disk_graph.cpp                                # File: on disk graph
│   │   │   └── parsed_graph_entry.cpp                           # File: parsed graph entry
│   │   ├── include/                                             # Include
│   │   │   ├── binder/                                          # Binder
│   │   │   │   ├── copy/                                        # Copy
│   │   │   │   │   ├── bound_copy_from.h                        # File: bound copy from
│   │   │   │   │   ├── bound_copy_to.h                          # File: bound copy to
│   │   │   │   │   ├── bound_query_scan_info.h                  # File: bound query scan info
│   │   │   │   │   └── index_look_up_info.h                     # File: index look up info
│   │   │   │   ├── ddl/                                         # Ddl
│   │   │   │   │   ├── bound_alter.h                            # File: bound alter
│   │   │   │   │   ├── bound_alter_info.h                       # File: bound alter info
│   │   │   │   │   ├── bound_create_index.h                     # File: bound create index
│   │   │   │   │   ├── bound_create_sequence.h                  # File: bound create sequence
│   │   │   │   │   ├── bound_create_sequence_info.h             # File: bound create sequence info
│   │   │   │   │   ├── bound_create_table.h                     # File: bound create table
│   │   │   │   │   ├── bound_create_table_info.h                # File: bound create table info
│   │   │   │   │   ├── bound_create_type.h                      # File: bound create type
│   │   │   │   │   ├── bound_drop.h                             # File: bound drop
│   │   │   │   │   └── property_definition.h                    # File: property definition
│   │   │   │   ├── expression/                                  # Expression
│   │   │   │   │   ├── aggregate_function_expression.h          # File: aggregate function expression
│   │   │   │   │   ├── case_expression.h                        # File: case expression
│   │   │   │   │   ├── expression.h                             # File: expression
│   │   │   │   │   ├── expression_util.h                        # File: expression util
│   │   │   │   │   ├── lambda_expression.h                      # File: lambda expression
│   │   │   │   │   ├── literal_expression.h                     # File: literal expression
│   │   │   │   │   ├── node_expression.h                        # File: node expression
│   │   │   │   │   ├── node_rel_expression.h                    # File: node rel expression
│   │   │   │   │   ├── parameter_expression.h                   # File: parameter expression
│   │   │   │   │   ├── path_expression.h                        # File: path expression
│   │   │   │   │   ├── property_expression.h                    # File: property expression
│   │   │   │   │   ├── rel_expression.h                         # File: rel expression
│   │   │   │   │   ├── scalar_function_expression.h             # File: scalar function expression
│   │   │   │   │   ├── subquery_expression.h                    # File: subquery expression
│   │   │   │   │   └── variable_expression.h                    # File: variable expression
│   │   │   │   ├── query/                                       # Query
│   │   │   │   │   ├── reading_clause/                          # Reading clause
│   │   │   │   │   │   ├── bound_join_hint.h                    # File: bound join hint
│   │   │   │   │   │   ├── bound_load_from.h                    # File: bound load from
│   │   │   │   │   │   ├── bound_match_clause.h                 # File: bound match clause
│   │   │   │   │   │   ├── bound_reading_clause.h               # File: bound reading clause
│   │   │   │   │   │   ├── bound_table_function_call.h          # File: bound table function call
│   │   │   │   │   │   └── bound_unwind_clause.h                # File: bound unwind clause
│   │   │   │   │   ├── return_with_clause/                      # Return with clause
│   │   │   │   │   │   ├── bound_projection_body.h              # File: bound projection body
│   │   │   │   │   │   ├── bound_return_clause.h                # File: bound return clause
│   │   │   │   │   │   └── bound_with_clause.h                  # File: bound with clause
│   │   │   │   │   ├── updating_clause/                         # Updating clause
│   │   │   │   │   │   ├── bound_delete_clause.h                # File: bound delete clause
│   │   │   │   │   │   ├── bound_delete_info.h                  # File: bound delete info
│   │   │   │   │   │   ├── bound_insert_clause.h                # File: bound insert clause
│   │   │   │   │   │   ├── bound_insert_info.h                  # File: bound insert info
│   │   │   │   │   │   ├── bound_merge_clause.h                 # File: bound merge clause
│   │   │   │   │   │   ├── bound_set_clause.h                   # File: bound set clause
│   │   │   │   │   │   ├── bound_set_info.h                     # File: bound set info
│   │   │   │   │   │   └── bound_updating_clause.h              # File: bound updating clause
│   │   │   │   │   ├── bound_regular_query.h                    # File: bound regular query
│   │   │   │   │   ├── normalized_query_part.h                  # File: normalized query part
│   │   │   │   │   ├── normalized_single_query.h                # File: normalized single query
│   │   │   │   │   ├── query_graph.h                            # File: query graph
│   │   │   │   │   └── query_graph_label_analyzer.h             # File: query graph label analyzer
│   │   │   │   ├── rewriter/                                    # Rewriter
│   │   │   │   │   ├── match_clause_pattern_label_rewriter.h    # File: match clause pattern label rewriter
│   │   │   │   │   ├── normalized_query_part_match_rewriter.h   # File: normalized query part match rewriter
│   │   │   │   │   └── with_clause_projection_rewriter.h        # File: with clause projection rewriter
│   │   │   │   ├── visitor/                                     # Visitor
│   │   │   │   │   ├── confidential_statement_analyzer.h        # File: confidential statement analyzer
│   │   │   │   │   ├── default_type_solver.h                    # File: default type solver
│   │   │   │   │   └── property_collector.h                     # File: property collector
│   │   │   │   ├── binder.h                                     # File: binder
│   │   │   │   ├── binder_scope.h                               # File: binder scope
│   │   │   │   ├── bound_analyze.h                              # File: bound analyze
│   │   │   │   ├── bound_attach_database.h                      # File: bound attach database
│   │   │   │   ├── bound_attach_info.h                          # File: bound attach info
│   │   │   │   ├── bound_create_macro.h                         # File: bound create macro
│   │   │   │   ├── bound_database_statement.h                   # File: bound database statement
│   │   │   │   ├── bound_detach_database.h                      # File: bound detach database
│   │   │   │   ├── bound_explain.h                              # File: bound explain
│   │   │   │   ├── bound_export_database.h                      # File: bound export database
│   │   │   │   ├── bound_extension_statement.h                  # File: bound extension statement
│   │   │   │   ├── bound_graph_statement.h                      # File: bound graph statement
│   │   │   │   ├── bound_import_database.h                      # File: bound import database
│   │   │   │   ├── bound_scan_source.h                          # File: bound scan source
│   │   │   │   ├── bound_standalone_call.h                      # File: bound standalone call
│   │   │   │   ├── bound_standalone_call_function.h             # File: bound standalone call function
│   │   │   │   ├── bound_statement.h                            # File: bound statement
│   │   │   │   ├── bound_statement_result.h                     # File: bound statement result
│   │   │   │   ├── bound_statement_rewriter.h                   # File: bound statement rewriter
│   │   │   │   ├── bound_statement_visitor.h                    # File: bound statement visitor
│   │   │   │   ├── bound_table_scan_info.h                      # File: bound table scan info
│   │   │   │   ├── bound_transaction_statement.h                # File: bound transaction statement
│   │   │   │   ├── bound_use_database.h                         # File: bound use database
│   │   │   │   ├── expression_binder.h                          # File: expression binder
│   │   │   │   └── expression_visitor.h                         # File: expression visitor
│   │   │   ├── c_api/                                           # C api
│   │   │   │   ├── helpers.h                                    # File: helpers
│   │   │   │   └── lbug.h                                       # File: lbug
│   │   │   ├── catalog/                                         # Catalog
│   │   │   │   ├── catalog_entry/                               # Catalog entry
│   │   │   │   │   ├── catalog_entry.h                          # File: catalog entry
│   │   │   │   │   ├── catalog_entry_type.h                     # File: catalog entry type
│   │   │   │   │   ├── dummy_catalog_entry.h                    # File: dummy catalog entry
│   │   │   │   │   ├── function_catalog_entry.h                 # File: function catalog entry
│   │   │   │   │   ├── graph_catalog_entry.h                    # File: graph catalog entry
│   │   │   │   │   ├── index_catalog_entry.h                    # File: index catalog entry
│   │   │   │   │   ├── node_table_catalog_entry.h               # File: node table catalog entry
│   │   │   │   │   ├── node_table_id_pair.h                     # File: node table id pair
│   │   │   │   │   ├── rel_group_catalog_entry.h                # File: rel group catalog entry
│   │   │   │   │   ├── scalar_macro_catalog_entry.h             # File: scalar macro catalog entry
│   │   │   │   │   ├── sequence_catalog_entry.h                 # File: sequence catalog entry
│   │   │   │   │   ├── table_catalog_entry.h                    # File: table catalog entry
│   │   │   │   │   └── type_catalog_entry.h                     # File: type catalog entry
│   │   │   │   ├── catalog.h                                    # File: catalog
│   │   │   │   ├── catalog_set.h                                # File: catalog set
│   │   │   │   └── property_definition_collection.h             # File: property definition collection
│   │   │   ├── common/                                          # Common
│   │   │   │   ├── arrow/                                       # Arrow
│   │   │   │   │   ├── arrow.h                                  # File: arrow
│   │   │   │   │   ├── arrow_buffer.h                           # File: arrow buffer
│   │   │   │   │   ├── arrow_converter.h                        # File: arrow converter
│   │   │   │   │   ├── arrow_nullmask_tree.h                    # File: arrow nullmask tree
│   │   │   │   │   ├── arrow_result_config.h                    # File: arrow result config
│   │   │   │   │   ├── arrow_row_batch.h                        # File: arrow row batch
│   │   │   │   │   └── arrow_schema_metadata.h                  # File: arrow schema metadata
│   │   │   │   ├── copier_config/                               # Copier config
│   │   │   │   │   ├── csv_reader_config.h                      # File: csv reader config
│   │   │   │   │   └── file_scan_info.h                         # File: file scan info
│   │   │   │   ├── data_chunk/                                  # Data chunk
│   │   │   │   │   ├── data_chunk.h                             # File: data chunk
│   │   │   │   │   ├── data_chunk_collection.h                  # File: data chunk collection
│   │   │   │   │   ├── data_chunk_state.h                       # File: data chunk state
│   │   │   │   │   └── sel_vector.h                             # File: sel vector
│   │   │   │   ├── enums/                                       # Enums
│   │   │   │   │   ├── accumulate_type.h                        # File: accumulate type
│   │   │   │   │   ├── alter_type.h                             # File: alter type
│   │   │   │   │   ├── clause_type.h                            # File: clause type
│   │   │   │   │   ├── column_evaluate_type.h                   # File: column evaluate type
│   │   │   │   │   ├── conflict_action.h                        # File: conflict action
│   │   │   │   │   ├── delete_type.h                            # File: delete type
│   │   │   │   │   ├── drop_type.h                              # File: drop type
│   │   │   │   │   ├── explain_type.h                           # File: explain type
│   │   │   │   │   ├── expression_type.h                        # File: expression type
│   │   │   │   │   ├── extend_direction.h                       # File: extend direction
│   │   │   │   │   ├── extend_direction_util.h                  # File: extend direction util
│   │   │   │   │   ├── join_type.h                              # File: join type
│   │   │   │   │   ├── path_semantic.h                          # File: path semantic
│   │   │   │   │   ├── query_rel_type.h                         # File: query rel type
│   │   │   │   │   ├── rel_direction.h                          # File: rel direction
│   │   │   │   │   ├── rel_multiplicity.h                       # File: rel multiplicity
│   │   │   │   │   ├── scan_source_type.h                       # File: scan source type
│   │   │   │   │   ├── statement_type.h                         # File: statement type
│   │   │   │   │   ├── storage_format.h                         # File: storage format
│   │   │   │   │   ├── subquery_type.h                          # File: subquery type
│   │   │   │   │   ├── table_type.h                             # File: table type
│   │   │   │   │   └── zone_map_check_result.h                  # File: zone map check result
│   │   │   │   ├── exception/                                   # Exception
│   │   │   │   │   ├── binder.h                                 # File: binder
│   │   │   │   │   ├── buffer_manager.h                         # File: buffer manager
│   │   │   │   │   ├── catalog.h                                # File: catalog
│   │   │   │   │   ├── checkpoint.h                             # File: checkpoint
│   │   │   │   │   ├── connection.h                             # File: connection
│   │   │   │   │   ├── conversion.h                             # File: conversion
│   │   │   │   │   ├── copy.h                                   # File: copy
│   │   │   │   │   ├── exception.h                              # File: exception
│   │   │   │   │   ├── extension.h                              # File: extension
│   │   │   │   │   ├── internal.h                               # File: internal
│   │   │   │   │   ├── interrupt.h                              # File: interrupt
│   │   │   │   │   ├── io.h                                     # File: io
│   │   │   │   │   ├── message.h                                # File: message
│   │   │   │   │   ├── not_implemented.h                        # File: not implemented
│   │   │   │   │   ├── overflow.h                               # File: overflow
│   │   │   │   │   ├── parser.h                                 # File: parser
│   │   │   │   │   ├── runtime.h                                # File: runtime
│   │   │   │   │   ├── storage.h                                # File: storage
│   │   │   │   │   ├── test.h                                   # File: test
│   │   │   │   │   └── transaction_manager.h                    # File: transaction manager
│   │   │   │   ├── file_system/                                 # File system
│   │   │   │   │   ├── compressed_file_system.h                 # File: compressed file system
│   │   │   │   │   ├── file_info.h                              # File: file info
│   │   │   │   │   ├── file_system.h                            # File: file system
│   │   │   │   │   ├── gzip_file_system.h                       # File: gzip file system
│   │   │   │   │   ├── local_file_system.h                      # File: local file system
│   │   │   │   │   └── virtual_file_system.h                    # File: virtual file system
│   │   │   │   ├── serializer/                                  # Serializer
│   │   │   │   │   ├── buffer_reader.h                          # File: buffer reader
│   │   │   │   │   ├── buffer_writer.h                          # File: buffer writer
│   │   │   │   │   ├── buffered_file.h                          # File: buffered file
│   │   │   │   │   ├── deserializer.h                           # File: deserializer
│   │   │   │   │   ├── in_mem_file_writer.h                     # File: in mem file writer
│   │   │   │   │   ├── reader.h                                 # File: reader
│   │   │   │   │   ├── serializer.h                             # File: serializer
│   │   │   │   │   └── writer.h                                 # File: writer
│   │   │   │   ├── simd/                                        # Simd
│   │   │   │   │   └── cpu_features.h                           # File: cpu features
│   │   │   │   ├── task_system/                                 # Task system
│   │   │   │   │   ├── progress_bar.h                           # File: progress bar
│   │   │   │   │   ├── progress_bar_display.h                   # File: progress bar display
│   │   │   │   │   ├── task.h                                   # File: task
│   │   │   │   │   ├── task_scheduler.h                         # File: task scheduler
│   │   │   │   │   └── terminal_progress_bar_display.h          # File: terminal progress bar display
│   │   │   │   ├── types/                                       # Types
│   │   │   │   │   ├── value/                                   # Value
│   │   │   │   │   │   ├── nested.h                             # File: nested
│   │   │   │   │   │   ├── node.h                               # File: node
│   │   │   │   │   │   ├── recursive_rel.h                      # File: recursive rel
│   │   │   │   │   │   ├── rel.h                                # File: rel
│   │   │   │   │   │   └── value.h                              # File: value
│   │   │   │   │   ├── blob.h                                   # File: blob
│   │   │   │   │   ├── cast_helpers.h                           # File: cast helpers
│   │   │   │   │   ├── date_t.h                                 # File: date t
│   │   │   │   │   ├── dtime_t.h                                # File: dtime t
│   │   │   │   │   ├── int128_t.h                               # File: int128 t
│   │   │   │   │   ├── internal_id_util.h                       # File: internal id util
│   │   │   │   │   ├── interval_t.h                             # File: interval t
│   │   │   │   │   ├── json_type.h                              # File: json type
│   │   │   │   │   ├── list_t.h                                 # File: list t
│   │   │   │   │   ├── string_t.h                               # File: string t
│   │   │   │   │   ├── timestamp_t.h                            # File: timestamp t
│   │   │   │   │   ├── types.h                                  # File: types
│   │   │   │   │   ├── uint128_t.h                              # File: uint128 t
│   │   │   │   │   └── uuid.h                                   # File: uuid
│   │   │   │   ├── vector/                                      # Vector
│   │   │   │   │   ├── auxiliary_buffer.h                       # File: auxiliary buffer
│   │   │   │   │   └── value_vector.h                           # File: value vector
│   │   │   │   ├── api.h                                        # File: api
│   │   │   │   ├── array_utils.h                                # File: array utils
│   │   │   │   ├── assert.h                                     # File: assert
│   │   │   │   ├── case_insensitive_map.h                       # File: case insensitive map
│   │   │   │   ├── cast.h                                       # File: cast
│   │   │   │   ├── checksum.h                                   # File: checksum
│   │   │   │   ├── concurrent_vector.h                          # File: concurrent vector
│   │   │   │   ├── constants.h                                  # File: constants
│   │   │   │   ├── copy_constructors.h                          # File: copy constructors
│   │   │   │   ├── counter.h                                    # File: counter
│   │   │   │   ├── database_lifecycle_manager.h                 # File: database lifecycle manager
│   │   │   │   ├── finally_wrapper.h                            # File: finally wrapper
│   │   │   │   ├── in_mem_overflow_buffer.h                     # File: in mem overflow buffer
│   │   │   │   ├── json.h                                       # File: json
│   │   │   │   ├── json_utils.h                                 # File: json utils
│   │   │   │   ├── mask.h                                       # File: mask
│   │   │   │   ├── md5.h                                        # File: md5
│   │   │   │   ├── metric.h                                     # File: metric
│   │   │   │   ├── mpsc_queue.h                                 # File: mpsc queue
│   │   │   │   ├── mutex.h                                      # File: mutex
│   │   │   │   ├── null_buffer.h                                # File: null buffer
│   │   │   │   ├── null_mask.h                                  # File: null mask
│   │   │   │   ├── numeric_utils.h                              # File: numeric utils
│   │   │   │   ├── partition_routing.h                          # File: partition routing
│   │   │   │   ├── partition_routing_hook.h                     # File: partition routing hook
│   │   │   │   ├── profiler.h                                   # File: profiler
│   │   │   │   ├── random_engine.h                              # File: random engine
│   │   │   │   ├── roaring_mask.h                               # File: roaring mask
│   │   │   │   ├── sha256.h                                     # File: sha256
│   │   │   │   ├── static_vector.h                              # File: static vector
│   │   │   │   ├── string_utils.h                               # File: string utils
│   │   │   │   ├── system_message.h                             # File: system message
│   │   │   │   ├── timer.h                                      # File: timer
│   │   │   │   ├── type_utils.h                                 # File: type utils
│   │   │   │   ├── uniq_lock.h                                  # File: uniq lock
│   │   │   │   ├── utils.h                                      # File: utils
│   │   │   │   └── windows_utils.h                              # File: windows utils
│   │   │   ├── expression_evaluator/                            # Expression evaluator
│   │   │   │   ├── case_evaluator.h                             # File: case evaluator
│   │   │   │   ├── expression_evaluator.h                       # File: expression evaluator
│   │   │   │   ├── expression_evaluator_utils.h                 # File: expression evaluator utils
│   │   │   │   ├── expression_evaluator_visitor.h               # File: expression evaluator visitor
│   │   │   │   ├── function_evaluator.h                         # File: function evaluator
│   │   │   │   ├── lambda_evaluator.h                           # File: lambda evaluator
│   │   │   │   ├── list_slice_info.h                            # File: list slice info
│   │   │   │   ├── literal_evaluator.h                          # File: literal evaluator
│   │   │   │   ├── path_evaluator.h                             # File: path evaluator
│   │   │   │   ├── pattern_evaluator.h                          # File: pattern evaluator
│   │   │   │   └── reference_evaluator.h                        # File: reference evaluator
│   │   │   ├── extension/                                       # Extension
│   │   │   │   ├── binder_extension.h                           # File: binder extension
│   │   │   │   ├── bound_extension_clause.h                     # File: bound extension clause
│   │   │   │   ├── catalog_extension.h                          # File: catalog extension
│   │   │   │   ├── extension.h                                  # File: extension
│   │   │   │   ├── extension_action.h                           # File: extension action
│   │   │   │   ├── extension_installer.h                        # File: extension installer
│   │   │   │   ├── extension_loader.h                           # File: extension loader
│   │   │   │   ├── extension_manager.h                          # File: extension manager
│   │   │   │   ├── extension_statement.h                        # File: extension statement
│   │   │   │   ├── loaded_extension.h                           # File: loaded extension
│   │   │   │   ├── logical_extension_clause.h                   # File: logical extension clause
│   │   │   │   ├── mapper_extension.h                           # File: mapper extension
│   │   │   │   ├── planner_extension.h                          # File: planner extension
│   │   │   │   └── transformer_extension.h                      # File: transformer extension
│   │   │   ├── function/                                        # Function
│   │   │   │   ├── aggregate/                                   # Aggregate
│   │   │   │   │   ├── avg.h                                    # File: avg
│   │   │   │   │   ├── base_count.h                             # File: base count
│   │   │   │   │   ├── comparison_funcs.h                       # File: comparison funcs
│   │   │   │   │   ├── conversion_funcs.h                       # File: conversion funcs
│   │   │   │   │   ├── count.h                                  # File: count
│   │   │   │   │   ├── count_star.h                             # File: count star
│   │   │   │   │   ├── histogram.h                              # File: histogram
│   │   │   │   │   ├── min_max.h                                # File: min max
│   │   │   │   │   ├── percentile_cont.h                        # File: percentile cont
│   │   │   │   │   ├── percentile_disc.h                        # File: percentile disc
│   │   │   │   │   └── sum.h                                    # File: sum
│   │   │   │   ├── arithmetic/                                  # Arithmetic
│   │   │   │   │   ├── abs.h                                    # File: abs
│   │   │   │   │   ├── add.h                                    # File: add
│   │   │   │   │   ├── arithmetic_functions.h                   # File: arithmetic functions
│   │   │   │   │   ├── divide.h                                 # File: divide
│   │   │   │   │   ├── modulo.h                                 # File: modulo
│   │   │   │   │   ├── multiply.h                               # File: multiply
│   │   │   │   │   ├── negate.h                                 # File: negate
│   │   │   │   │   ├── subtract.h                               # File: subtract
│   │   │   │   │   └── vector_arithmetic_functions.h            # File: vector arithmetic functions
│   │   │   │   ├── array/                                       # Array
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── array_cosine_similarity.h            # File: array cosine similarity
│   │   │   │   │   │   ├── array_cross_product.h                # File: array cross product
│   │   │   │   │   │   ├── array_distance.h                     # File: array distance
│   │   │   │   │   │   ├── array_inner_product.h                # File: array inner product
│   │   │   │   │   │   └── array_squared_distance.h             # File: array squared distance
│   │   │   │   │   └── vector_array_functions.h                 # File: vector array functions
│   │   │   │   ├── blob/                                        # Blob
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── decode_function.h                    # File: decode function
│   │   │   │   │   │   ├── encode_function.h                    # File: encode function
│   │   │   │   │   │   └── octet_length_function.h              # File: octet length function
│   │   │   │   │   └── vector_blob_functions.h                  # File: vector blob functions
│   │   │   │   ├── boolean/                                     # Boolean
│   │   │   │   │   ├── boolean_function_executor.h              # File: boolean function executor
│   │   │   │   │   ├── boolean_functions.h                      # File: boolean functions
│   │   │   │   │   └── vector_boolean_functions.h               # File: vector boolean functions
│   │   │   │   ├── cast/                                        # Cast
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── cast_array.h                         # File: cast array
│   │   │   │   │   │   ├── cast_decimal.h                       # File: cast decimal
│   │   │   │   │   │   ├── cast_from_string_functions.h         # File: cast from string functions
│   │   │   │   │   │   ├── cast_functions.h                     # File: cast functions
│   │   │   │   │   │   ├── cast_string_non_nested_functions.h   # File: cast string non nested functions
│   │   │   │   │   │   ├── numeric_cast.h                       # File: numeric cast
│   │   │   │   │   │   └── numeric_limits.h                     # File: numeric limits
│   │   │   │   │   ├── cast_function_bind_data.h                # File: cast function bind data
│   │   │   │   │   ├── cast_union_bind_data.h                   # File: cast union bind data
│   │   │   │   │   └── vector_cast_functions.h                  # File: vector cast functions
│   │   │   │   ├── comparison/                                  # Comparison
│   │   │   │   │   ├── comparison_functions.h                   # File: comparison functions
│   │   │   │   │   ├── comparison_operation.h                   # File: comparison operation
│   │   │   │   │   ├── simd_filter.h                            # File: simd filter
│   │   │   │   │   └── vector_comparison_functions.h            # File: vector comparison functions
│   │   │   │   ├── date/                                        # Date
│   │   │   │   │   ├── date_functions.h                         # File: date functions
│   │   │   │   │   └── vector_date_functions.h                  # File: vector date functions
│   │   │   │   ├── export/                                      # Export
│   │   │   │   │   └── export_function.h                        # File: export function
│   │   │   │   ├── gds/                                         # Gds
│   │   │   │   │   ├── auxiliary_state/                         # Auxiliary state
│   │   │   │   │   │   ├── gds_auxilary_state.h                 # File: gds auxilary state
│   │   │   │   │   │   └── path_auxiliary_state.h               # File: path auxiliary state
│   │   │   │   │   ├── bfs_graph.h                              # File: bfs graph
│   │   │   │   │   ├── compute.h                                # File: compute
│   │   │   │   │   ├── density_state.h                          # File: density state
│   │   │   │   │   ├── frontier_morsel.h                        # File: frontier morsel
│   │   │   │   │   ├── gds.h                                    # File: gds
│   │   │   │   │   ├── gds_frontier.h                           # File: gds frontier
│   │   │   │   │   ├── gds_function_collection.h                # File: gds function collection
│   │   │   │   │   ├── gds_object_manager.h                     # File: gds object manager
│   │   │   │   │   ├── gds_state.h                              # File: gds state
│   │   │   │   │   ├── gds_task.h                               # File: gds task
│   │   │   │   │   ├── gds_utils.h                              # File: gds utils
│   │   │   │   │   ├── gds_vertex_compute.h                     # File: gds vertex compute
│   │   │   │   │   ├── rec_joins.h                              # File: rec joins
│   │   │   │   │   ├── rj_output_writer.h                       # File: rj output writer
│   │   │   │   │   └── weight_utils.h                           # File: weight utils
│   │   │   │   ├── hash/                                        # Hash
│   │   │   │   │   ├── hash_functions.h                         # File: hash functions
│   │   │   │   │   └── vector_hash_functions.h                  # File: vector hash functions
│   │   │   │   ├── internal_id/                                 # Internal id
│   │   │   │   │   └── vector_internal_id_functions.h           # File: vector internal id functions
│   │   │   │   ├── interval/                                    # Interval
│   │   │   │   │   ├── interval_functions.h                     # File: interval functions
│   │   │   │   │   └── vector_interval_functions.h              # File: vector interval functions
│   │   │   │   ├── list/                                        # List
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── base_list_sort_function.h            # File: base list sort function
│   │   │   │   │   │   ├── list_concat_function.h               # File: list concat function
│   │   │   │   │   │   ├── list_extract_function.h              # File: list extract function
│   │   │   │   │   │   ├── list_function_utils.h                # File: list function utils
│   │   │   │   │   │   ├── list_len_function.h                  # File: list len function
│   │   │   │   │   │   ├── list_position_function.h             # File: list position function
│   │   │   │   │   │   ├── list_reverse_sort_function.h         # File: list reverse sort function
│   │   │   │   │   │   ├── list_sort_function.h                 # File: list sort function
│   │   │   │   │   │   └── list_unique_function.h               # File: list unique function
│   │   │   │   │   └── vector_list_functions.h                  # File: vector list functions
│   │   │   │   ├── map/                                         # Map
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── base_map_extract_function.h          # File: base map extract function
│   │   │   │   │   │   ├── map_creation_function.h              # File: map creation function
│   │   │   │   │   │   ├── map_extract_function.h               # File: map extract function
│   │   │   │   │   │   ├── map_keys_function.h                  # File: map keys function
│   │   │   │   │   │   └── map_values_function.h                # File: map values function
│   │   │   │   │   └── vector_map_functions.h                   # File: vector map functions
│   │   │   │   ├── null/                                        # Null
│   │   │   │   │   ├── null_function_executor.h                 # File: null function executor
│   │   │   │   │   ├── null_functions.h                         # File: null functions
│   │   │   │   │   └── vector_null_functions.h                  # File: vector null functions
│   │   │   │   ├── path/                                        # Path
│   │   │   │   │   ├── path_function_executor.h                 # File: path function executor
│   │   │   │   │   └── vector_path_functions.h                  # File: vector path functions
│   │   │   │   ├── schema/                                      # Schema
│   │   │   │   │   ├── offset_functions.h                       # File: offset functions
│   │   │   │   │   └── vector_node_rel_functions.h              # File: vector node rel functions
│   │   │   │   ├── sequence/                                    # Sequence
│   │   │   │   │   └── sequence_functions.h                     # File: sequence functions
│   │   │   │   ├── string/                                      # String
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   ├── array_extract_function.h             # File: array extract function
│   │   │   │   │   │   ├── base_lower_upper_function.h          # File: base lower upper function
│   │   │   │   │   │   ├── base_pad_function.h                  # File: base pad function
│   │   │   │   │   │   ├── base_regexp_function.h               # File: base regexp function
│   │   │   │   │   │   ├── base_str_function.h                  # File: base str function
│   │   │   │   │   │   ├── contains_function.h                  # File: contains function
│   │   │   │   │   │   ├── ends_with_function.h                 # File: ends with function
│   │   │   │   │   │   ├── find_function.h                      # File: find function
│   │   │   │   │   │   ├── left_operation.h                     # File: left operation
│   │   │   │   │   │   ├── lower_function.h                     # File: lower function
│   │   │   │   │   │   ├── lpad_function.h                      # File: lpad function
│   │   │   │   │   │   ├── ltrim_function.h                     # File: ltrim function
│   │   │   │   │   │   ├── pad_function.h                       # File: pad function
│   │   │   │   │   │   ├── regexp_extract_all_function.h        # File: regexp extract all function
│   │   │   │   │   │   ├── regexp_extract_function.h            # File: regexp extract function
│   │   │   │   │   │   ├── regexp_matches_function.h            # File: regexp matches function
│   │   │   │   │   │   ├── regexp_split_to_array_function.h     # File: regexp split to array function
│   │   │   │   │   │   ├── repeat_function.h                    # File: repeat function
│   │   │   │   │   │   ├── reverse_function.h                   # File: reverse function
│   │   │   │   │   │   ├── right_function.h                     # File: right function
│   │   │   │   │   │   ├── rpad_function.h                      # File: rpad function
│   │   │   │   │   │   ├── rtrim_function.h                     # File: rtrim function
│   │   │   │   │   │   ├── starts_with_function.h               # File: starts with function
│   │   │   │   │   │   ├── substr_function.h                    # File: substr function
│   │   │   │   │   │   ├── trim_function.h                      # File: trim function
│   │   │   │   │   │   └── upper_function.h                     # File: upper function
│   │   │   │   │   └── vector_string_functions.h                # File: vector string functions
│   │   │   │   ├── struct/                                      # Struct
│   │   │   │   │   └── vector_struct_functions.h                # File: vector struct functions
│   │   │   │   ├── table/                                       # Table
│   │   │   │   │   ├── bind_data.h                              # File: bind data
│   │   │   │   │   ├── bind_input.h                             # File: bind input
│   │   │   │   │   ├── optional_params.h                        # File: optional params
│   │   │   │   │   ├── scan_file_function.h                     # File: scan file function
│   │   │   │   │   ├── scan_replacement.h                       # File: scan replacement
│   │   │   │   │   ├── simple_table_function.h                  # File: simple table function
│   │   │   │   │   ├── standalone_call_function.h               # File: standalone call function
│   │   │   │   │   └── table_function.h                         # File: table function
│   │   │   │   ├── timestamp/                                   # Timestamp
│   │   │   │   │   ├── time_bucket.h                            # File: time bucket
│   │   │   │   │   ├── timestamp_function.h                     # File: timestamp function
│   │   │   │   │   └── vector_timestamp_functions.h             # File: vector timestamp functions
│   │   │   │   ├── union/                                       # Union
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   └── union_tag.h                          # File: union tag
│   │   │   │   │   └── vector_union_functions.h                 # File: vector union functions
│   │   │   │   ├── utility/                                     # Utility
│   │   │   │   │   ├── function_string_bind_data.h              # File: function string bind data
│   │   │   │   │   └── vector_utility_functions.h               # File: vector utility functions
│   │   │   │   ├── uuid/                                        # Uuid
│   │   │   │   │   ├── functions/                               # Functions
│   │   │   │   │   │   └── gen_random_uuid.h                    # File: gen random uuid
│   │   │   │   │   └── vector_uuid_functions.h                  # File: vector uuid functions
│   │   │   │   ├── aggregate_function.h                         # File: aggregate function
│   │   │   │   ├── binary_function_executor.h                   # File: binary function executor
│   │   │   │   ├── built_in_function_utils.h                    # File: built in function utils
│   │   │   │   ├── const_function_executor.h                    # File: const function executor
│   │   │   │   ├── function.h                                   # File: function
│   │   │   │   ├── function_collection.h                        # File: function collection
│   │   │   │   ├── pointer_function_executor.h                  # File: pointer function executor
│   │   │   │   ├── rewrite_function.h                           # File: rewrite function
│   │   │   │   ├── scalar_function.h                            # File: scalar function
│   │   │   │   ├── scalar_macro_function.h                      # File: scalar macro function
│   │   │   │   ├── ternary_function_executor.h                  # File: ternary function executor
│   │   │   │   ├── udf_function.h                               # File: udf function
│   │   │   │   └── unary_function_executor.h                    # File: unary function executor
│   │   │   ├── graph/                                           # Graph
│   │   │   │   ├── graph.h                                      # File: graph
│   │   │   │   ├── graph_entry.h                                # File: graph entry
│   │   │   │   ├── graph_entry_set.h                            # File: graph entry set
│   │   │   │   ├── on_disk_graph.h                              # File: on disk graph
│   │   │   │   └── parsed_graph_entry.h                         # File: parsed graph entry
│   │   │   ├── main/                                            # Main
│   │   │   │   ├── query_result/                                # Query result
│   │   │   │   │   ├── arrow_query_result.h                     # File: arrow query result
│   │   │   │   │   └── materialized_query_result.h              # File: materialized query result
│   │   │   │   ├── attached_database.h                          # File: attached database
│   │   │   │   ├── client_config.h                              # File: client config
│   │   │   │   ├── client_context.h                             # File: client context
│   │   │   │   ├── connection.h                                 # File: connection
│   │   │   │   ├── database.h                                   # File: database
│   │   │   │   ├── database_manager.h                           # File: database manager
│   │   │   │   ├── db_config.h                                  # File: db config
│   │   │   │   ├── lbug.h                                       # File: lbug
│   │   │   │   ├── lbug_fwd.h                                   # File: lbug fwd
│   │   │   │   ├── option.h                                     # File: option
│   │   │   │   ├── plan_printer.h                               # File: plan printer
│   │   │   │   ├── prepared_statement.h                         # File: prepared statement
│   │   │   │   ├── prepared_statement_manager.h                 # File: prepared statement manager
│   │   │   │   ├── query_result.h                               # File: query result
│   │   │   │   ├── query_summary.h                              # File: query summary
│   │   │   │   ├── settings.h                                   # File: settings
│   │   │   │   ├── storage_driver.h                             # File: storage driver
│   │   │   │   └── version.h                                    # File: version
│   │   │   ├── optimizer/                                       # Optimizer
│   │   │   │   ├── acc_hash_join_optimizer.h                    # File: acc hash join optimizer
│   │   │   │   ├── agg_key_dependency_optimizer.h               # File: agg key dependency optimizer
│   │   │   │   ├── bool_folding_optimizer.h                     # File: bool folding optimizer
│   │   │   │   ├── cardinality_updater.h                        # File: cardinality updater
│   │   │   │   ├── correlated_subquery_unnest_solver.h          # File: correlated subquery unnest solver
│   │   │   │   ├── count_rel_table_optimizer.h                  # File: count rel table optimizer
│   │   │   │   ├── factorization_rewriter.h                     # File: factorization rewriter
│   │   │   │   ├── filter_push_down_optimizer.h                 # File: filter push down optimizer
│   │   │   │   ├── foreign_join_push_down_optimizer.h           # File: foreign join push down optimizer
│   │   │   │   ├── limit_push_down_optimizer.h                  # File: limit push down optimizer
│   │   │   │   ├── logical_operator_collector.h                 # File: logical operator collector
│   │   │   │   ├── logical_operator_visitor.h                   # File: logical operator visitor
│   │   │   │   ├── optimizer.h                                  # File: optimizer
│   │   │   │   ├── order_by_push_down_optimizer.h               # File: order by push down optimizer
│   │   │   │   ├── projection_push_down_optimizer.h             # File: projection push down optimizer
│   │   │   │   ├── remove_factorization_rewriter.h              # File: remove factorization rewriter
│   │   │   │   ├── remove_unnecessary_distinct_optimizer.h      # File: remove unnecessary distinct optimizer
│   │   │   │   ├── remove_unnecessary_join_optimizer.h          # File: remove unnecessary join optimizer
│   │   │   │   ├── remove_unnecessary_order_by_optimizer.h      # File: remove unnecessary order by optimizer
│   │   │   │   ├── schema_populator.h                           # File: schema populator
│   │   │   │   ├── top_k_optimizer.h                            # File: top k optimizer
│   │   │   │   └── unwind_dedup_optimizer.h                     # File: unwind dedup optimizer
│   │   │   ├── parser/                                          # Parser
│   │   │   │   ├── antlr_parser/                                # Antlr parser
│   │   │   │   │   ├── lbug_cypher_parser.h                     # File: lbug cypher parser
│   │   │   │   │   ├── parser_error_listener.h                  # File: parser error listener
│   │   │   │   │   └── parser_error_strategy.h                  # File: parser error strategy
│   │   │   │   ├── ddl/                                         # Ddl
│   │   │   │   │   ├── alter.h                                  # File: alter
│   │   │   │   │   ├── alter_info.h                             # File: alter info
│   │   │   │   │   ├── create_index.h                           # File: create index
│   │   │   │   │   ├── create_sequence.h                        # File: create sequence
│   │   │   │   │   ├── create_sequence_info.h                   # File: create sequence info
│   │   │   │   │   ├── create_table.h                           # File: create table
│   │   │   │   │   ├── create_table_info.h                      # File: create table info
│   │   │   │   │   ├── create_type.h                            # File: create type
│   │   │   │   │   ├── drop.h                                   # File: drop
│   │   │   │   │   ├── drop_info.h                              # File: drop info
│   │   │   │   │   └── parsed_property_definition.h             # File: parsed property definition
│   │   │   │   ├── expression/                                  # Expression
│   │   │   │   │   ├── parsed_case_expression.h                 # File: parsed case expression
│   │   │   │   │   ├── parsed_expression.h                      # File: parsed expression
│   │   │   │   │   ├── parsed_expression_visitor.h              # File: parsed expression visitor
│   │   │   │   │   ├── parsed_function_expression.h             # File: parsed function expression
│   │   │   │   │   ├── parsed_lambda_expression.h               # File: parsed lambda expression
│   │   │   │   │   ├── parsed_literal_expression.h              # File: parsed literal expression
│   │   │   │   │   ├── parsed_parameter_expression.h            # File: parsed parameter expression
│   │   │   │   │   ├── parsed_property_expression.h             # File: parsed property expression
│   │   │   │   │   ├── parsed_subquery_expression.h             # File: parsed subquery expression
│   │   │   │   │   └── parsed_variable_expression.h             # File: parsed variable expression
│   │   │   │   ├── parsed_data/                                 # Parsed data
│   │   │   │   │   └── attach_info.h                            # File: attach info
│   │   │   │   ├── query/                                       # Query
│   │   │   │   │   ├── graph_pattern/                           # Graph pattern
│   │   │   │   │   │   ├── node_pattern.h                       # File: node pattern
│   │   │   │   │   │   ├── pattern_element.h                    # File: pattern element
│   │   │   │   │   │   ├── pattern_element_chain.h              # File: pattern element chain
│   │   │   │   │   │   └── rel_pattern.h                        # File: rel pattern
│   │   │   │   │   ├── reading_clause/                          # Reading clause
│   │   │   │   │   │   ├── in_query_call_clause.h               # File: in query call clause
│   │   │   │   │   │   ├── join_hint.h                          # File: join hint
│   │   │   │   │   │   ├── load_from.h                          # File: load from
│   │   │   │   │   │   ├── match_clause.h                       # File: match clause
│   │   │   │   │   │   ├── reading_clause.h                     # File: reading clause
│   │   │   │   │   │   ├── unwind_clause.h                      # File: unwind clause
│   │   │   │   │   │   └── yield_variable.h                     # File: yield variable
│   │   │   │   │   ├── return_with_clause/                      # Return with clause
│   │   │   │   │   │   ├── projection_body.h                    # File: projection body
│   │   │   │   │   │   ├── return_clause.h                      # File: return clause
│   │   │   │   │   │   └── with_clause.h                        # File: with clause
│   │   │   │   │   ├── updating_clause/                         # Updating clause
│   │   │   │   │   │   ├── delete_clause.h                      # File: delete clause
│   │   │   │   │   │   ├── insert_clause.h                      # File: insert clause
│   │   │   │   │   │   ├── merge_clause.h                       # File: merge clause
│   │   │   │   │   │   ├── set_clause.h                         # File: set clause
│   │   │   │   │   │   └── updating_clause.h                    # File: updating clause
│   │   │   │   │   ├── query_part.h                             # File: query part
│   │   │   │   │   ├── regular_query.h                          # File: regular query
│   │   │   │   │   └── single_query.h                           # File: single query
│   │   │   │   ├── visitor/                                     # Visitor
│   │   │   │   │   ├── standalone_call_rewriter.h               # File: standalone call rewriter
│   │   │   │   │   └── statement_read_write_analyzer.h          # File: statement read write analyzer
│   │   │   │   ├── analyze_statement.h                          # File: analyze statement
│   │   │   │   ├── attach_database.h                            # File: attach database
│   │   │   │   ├── copy.h                                       # File: copy
│   │   │   │   ├── create_macro.h                               # File: create macro
│   │   │   │   ├── database_statement.h                         # File: database statement
│   │   │   │   ├── detach_database.h                            # File: detach database
│   │   │   │   ├── explain_statement.h                          # File: explain statement
│   │   │   │   ├── extension_statement.h                        # File: extension statement
│   │   │   │   ├── graph_statement.h                            # File: graph statement
│   │   │   │   ├── parsed_statement_visitor.h                   # File: parsed statement visitor
│   │   │   │   ├── parser.h                                     # File: parser
│   │   │   │   ├── port_db.h                                    # File: port db
│   │   │   │   ├── scan_source.h                                # File: scan source
│   │   │   │   ├── standalone_call.h                            # File: standalone call
│   │   │   │   ├── standalone_call_function.h                   # File: standalone call function
│   │   │   │   ├── statement.h                                  # File: statement
│   │   │   │   ├── transaction_statement.h                      # File: transaction statement
│   │   │   │   ├── transformer.h                                # File: transformer
│   │   │   │   └── use_database.h                               # File: use database
│   │   │   ├── planner/                                         # Planner
│   │   │   │   ├── join_order/                                  # Join order
│   │   │   │   │   ├── cardinality_estimator.h                  # File: cardinality estimator
│   │   │   │   │   ├── cost_model.h                             # File: cost model
│   │   │   │   │   ├── join_order_util.h                        # File: join order util
│   │   │   │   │   ├── join_plan_solver.h                       # File: join plan solver
│   │   │   │   │   ├── join_tree.h                              # File: join tree
│   │   │   │   │   └── join_tree_constructor.h                  # File: join tree constructor
│   │   │   │   ├── operator/                                    # Operator
│   │   │   │   │   ├── ddl/                                     # Ddl
│   │   │   │   │   │   ├── logical_alter.h                      # File: logical alter
│   │   │   │   │   │   ├── logical_create_index.h               # File: logical create index
│   │   │   │   │   │   ├── logical_create_sequence.h            # File: logical create sequence
│   │   │   │   │   │   ├── logical_create_table.h               # File: logical create table
│   │   │   │   │   │   ├── logical_create_type.h                # File: logical create type
│   │   │   │   │   │   └── logical_drop.h                       # File: logical drop
│   │   │   │   │   ├── extend/                                  # Extend
│   │   │   │   │   │   ├── base_logical_extend.h                # File: base logical extend
│   │   │   │   │   │   ├── logical_extend.h                     # File: logical extend
│   │   │   │   │   │   ├── logical_packed_extend.h              # File: logical packed extend
│   │   │   │   │   │   ├── logical_recursive_extend.h           # File: logical recursive extend
│   │   │   │   │   │   └── recursive_join_type.h                # File: recursive join type
│   │   │   │   │   ├── factorization/                           # Factorization
│   │   │   │   │   │   ├── flatten_resolver.h                   # File: flatten resolver
│   │   │   │   │   │   └── sink_util.h                          # File: sink util
│   │   │   │   │   ├── persistent/                              # Persistent
│   │   │   │   │   │   ├── logical_copy_from.h                  # File: logical copy from
│   │   │   │   │   │   ├── logical_copy_to.h                    # File: logical copy to
│   │   │   │   │   │   ├── logical_delete.h                     # File: logical delete
│   │   │   │   │   │   ├── logical_insert.h                     # File: logical insert
│   │   │   │   │   │   ├── logical_merge.h                      # File: logical merge
│   │   │   │   │   │   └── logical_set.h                        # File: logical set
│   │   │   │   │   ├── scan/                                    # Scan
│   │   │   │   │   │   ├── logical_count_rel_table.h            # File: logical count rel table
│   │   │   │   │   │   ├── logical_dummy_scan.h                 # File: logical dummy scan
│   │   │   │   │   │   ├── logical_expressions_scan.h           # File: logical expressions scan
│   │   │   │   │   │   ├── logical_index_look_up.h              # File: logical index look up
│   │   │   │   │   │   ├── logical_query_primary_key_lookup.h   # File: logical query primary key lookup
│   │   │   │   │   │   ├── logical_reachable_count.h            # File: logical reachable count
│   │   │   │   │   │   ├── logical_rel_degree_table.h           # File: logical rel degree table
│   │   │   │   │   │   └── logical_scan_node_table.h            # File: logical scan node table
│   │   │   │   │   ├── simple/                                  # Simple
│   │   │   │   │   │   ├── logical_analyze.h                    # File: logical analyze
│   │   │   │   │   │   ├── logical_attach_database.h            # File: logical attach database
│   │   │   │   │   │   ├── logical_detach_database.h            # File: logical detach database
│   │   │   │   │   │   ├── logical_export_db.h                  # File: logical export db
│   │   │   │   │   │   ├── logical_extension.h                  # File: logical extension
│   │   │   │   │   │   ├── logical_graph.h                      # File: logical graph
│   │   │   │   │   │   ├── logical_import_db.h                  # File: logical import db
│   │   │   │   │   │   ├── logical_simple.h                     # File: logical simple
│   │   │   │   │   │   └── logical_use_database.h               # File: logical use database
│   │   │   │   │   ├── sip/                                     # Sip
│   │   │   │   │   │   ├── logical_semi_masker.h                # File: logical semi masker
│   │   │   │   │   │   ├── semi_mask_target_type.h              # File: semi mask target type
│   │   │   │   │   │   └── side_way_info_passing.h              # File: side way info passing
│   │   │   │   │   ├── logical_accumulate.h                     # File: logical accumulate
│   │   │   │   │   ├── logical_aggregate.h                      # File: logical aggregate
│   │   │   │   │   ├── logical_create_macro.h                   # File: logical create macro
│   │   │   │   │   ├── logical_cross_product.h                  # File: logical cross product
│   │   │   │   │   ├── logical_distinct.h                       # File: logical distinct
│   │   │   │   │   ├── logical_dummy_sink.h                     # File: logical dummy sink
│   │   │   │   │   ├── logical_empty_result.h                   # File: logical empty result
│   │   │   │   │   ├── logical_explain.h                        # File: logical explain
│   │   │   │   │   ├── logical_filter.h                         # File: logical filter
│   │   │   │   │   ├── logical_flatten.h                        # File: logical flatten
│   │   │   │   │   ├── logical_hash_join.h                      # File: logical hash join
│   │   │   │   │   ├── logical_intersect.h                      # File: logical intersect
│   │   │   │   │   ├── logical_limit.h                          # File: logical limit
│   │   │   │   │   ├── logical_multiplcity_reducer.h            # File: logical multiplcity reducer
│   │   │   │   │   ├── logical_node_label_filter.h              # File: logical node label filter
│   │   │   │   │   ├── logical_noop.h                           # File: logical noop
│   │   │   │   │   ├── logical_operator.h                       # File: logical operator
│   │   │   │   │   ├── logical_order_by.h                       # File: logical order by
│   │   │   │   │   ├── logical_partitioner.h                    # File: logical partitioner
│   │   │   │   │   ├── logical_path_property_probe.h            # File: logical path property probe
│   │   │   │   │   ├── logical_plan.h                           # File: logical plan
│   │   │   │   │   ├── logical_plan_util.h                      # File: logical plan util
│   │   │   │   │   ├── logical_projection.h                     # File: logical projection
│   │   │   │   │   ├── logical_standalone_call.h                # File: logical standalone call
│   │   │   │   │   ├── logical_table_function_call.h            # File: logical table function call
│   │   │   │   │   ├── logical_transaction.h                    # File: logical transaction
│   │   │   │   │   ├── logical_union.h                          # File: logical union
│   │   │   │   │   ├── logical_unwind.h                         # File: logical unwind
│   │   │   │   │   ├── logical_unwind_deduplicate.h             # File: logical unwind deduplicate
│   │   │   │   │   ├── operator_print_info.h                    # File: operator print info
│   │   │   │   │   └── schema.h                                 # File: schema
│   │   │   │   ├── join_order_enumerator_context.h              # File: join order enumerator context
│   │   │   │   ├── planner.h                                    # File: planner
│   │   │   │   └── subplans_table.h                             # File: subplans table
│   │   │   ├── processor/                                       # Processor
│   │   │   │   ├── operator/                                    # Operator
│   │   │   │   │   ├── aggregate/                               # Aggregate
│   │   │   │   │   │   ├── aggregate_hash_table.h               # File: aggregate hash table
│   │   │   │   │   │   ├── aggregate_input.h                    # File: aggregate input
│   │   │   │   │   │   ├── base_aggregate.h                     # File: base aggregate
│   │   │   │   │   │   ├── base_aggregate_scan.h                # File: base aggregate scan
│   │   │   │   │   │   ├── hash_aggregate.h                     # File: hash aggregate
│   │   │   │   │   │   ├── hash_aggregate_scan.h                # File: hash aggregate scan
│   │   │   │   │   │   ├── packed_filtered_count.h              # File: packed filtered count
│   │   │   │   │   │   ├── simple_aggregate.h                   # File: simple aggregate
│   │   │   │   │   │   └── simple_aggregate_scan.h              # File: simple aggregate scan
│   │   │   │   │   ├── ddl/                                     # Ddl
│   │   │   │   │   │   ├── alter.h                              # File: alter
│   │   │   │   │   │   ├── create_index.h                       # File: create index
│   │   │   │   │   │   ├── create_sequence.h                    # File: create sequence
│   │   │   │   │   │   ├── create_table.h                       # File: create table
│   │   │   │   │   │   ├── create_type.h                        # File: create type
│   │   │   │   │   │   └── drop.h                               # File: drop
│   │   │   │   │   ├── hash_join/                               # Hash join
│   │   │   │   │   │   ├── hash_join_build.h                    # File: hash join build
│   │   │   │   │   │   ├── hash_join_probe.h                    # File: hash join probe
│   │   │   │   │   │   └── join_hash_table.h                    # File: join hash table
│   │   │   │   │   ├── intersect/                               # Intersect
│   │   │   │   │   │   ├── intersect.h                          # File: intersect
│   │   │   │   │   │   └── intersect_build.h                    # File: intersect build
│   │   │   │   │   ├── macro/                                   # Macro
│   │   │   │   │   │   └── create_macro.h                       # File: create macro
│   │   │   │   │   ├── order_by/                                # Order by
│   │   │   │   │   │   ├── key_block_merger.h                   # File: key block merger
│   │   │   │   │   │   ├── order_by.h                           # File: order by
│   │   │   │   │   │   ├── order_by_data_info.h                 # File: order by data info
│   │   │   │   │   │   ├── order_by_key_encoder.h               # File: order by key encoder
│   │   │   │   │   │   ├── order_by_merge.h                     # File: order by merge
│   │   │   │   │   │   ├── order_by_scan.h                      # File: order by scan
│   │   │   │   │   │   ├── radix_sort.h                         # File: radix sort
│   │   │   │   │   │   ├── sort_state.h                         # File: sort state
│   │   │   │   │   │   ├── top_k.h                              # File: top k
│   │   │   │   │   │   └── top_k_scanner.h                      # File: top k scanner
│   │   │   │   │   ├── persistent/                              # Persistent
│   │   │   │   │   │   ├── reader/                              # Reader
│   │   │   │   │   │   │   ├── csv/                             # Csv
│   │   │   │   │   │   │   │   ├── base_csv_reader.h            # File: base csv reader
│   │   │   │   │   │   │   │   ├── csv_boundary_scanner.h       # File: csv boundary scanner
│   │   │   │   │   │   │   │   ├── dialect_detection.h          # File: dialect detection
│   │   │   │   │   │   │   │   ├── driver.h                     # File: driver
│   │   │   │   │   │   │   │   ├── parallel_csv_reader.h        # File: parallel csv reader
│   │   │   │   │   │   │   │   └── serial_csv_reader.h          # File: serial csv reader
│   │   │   │   │   │   │   ├── npy/                             # Npy
│   │   │   │   │   │   │   │   └── npy_reader.h                 # File: npy reader
│   │   │   │   │   │   │   ├── parquet/                         # Parquet
│   │   │   │   │   │   │   │   ├── boolean_column_reader.h      # File: boolean column reader
│   │   │   │   │   │   │   │   ├── callback_column_reader.h     # File: callback column reader
│   │   │   │   │   │   │   │   ├── column_reader.h              # File: column reader
│   │   │   │   │   │   │   │   ├── decode_utils.h               # File: decode utils
│   │   │   │   │   │   │   │   ├── interval_column_reader.h     # File: interval column reader
│   │   │   │   │   │   │   │   ├── list_column_reader.h         # File: list column reader
│   │   │   │   │   │   │   │   ├── parquet_dbp_decoder.h        # File: parquet dbp decoder
│   │   │   │   │   │   │   │   ├── parquet_reader.h             # File: parquet reader
│   │   │   │   │   │   │   │   ├── parquet_rle_bp_decoder.h     # File: parquet rle bp decoder
│   │   │   │   │   │   │   │   ├── parquet_timestamp.h          # File: parquet timestamp
│   │   │   │   │   │   │   │   ├── resizable_buffer.h           # File: resizable buffer
│   │   │   │   │   │   │   │   ├── string_column_reader.h       # File: string column reader
│   │   │   │   │   │   │   │   ├── struct_column_reader.h       # File: struct column reader
│   │   │   │   │   │   │   │   ├── templated_column_reader.h    # File: templated column reader
│   │   │   │   │   │   │   │   ├── thrift_tools.h               # File: thrift tools
│   │   │   │   │   │   │   │   └── uuid_column_reader.h         # File: uuid column reader
│   │   │   │   │   │   │   ├── copy_from_error.h                # File: copy from error
│   │   │   │   │   │   │   ├── file_error_handler.h             # File: file error handler
│   │   │   │   │   │   │   └── reader_bind_utils.h              # File: reader bind utils
│   │   │   │   │   │   ├── writer/                              # Writer
│   │   │   │   │   │   │   └── parquet/                         # Parquet
│   │   │   │   │   │   │       ├── basic_column_writer.h        # File: basic column writer
│   │   │   │   │   │   │       ├── boolean_column_writer.h      # File: boolean column writer
│   │   │   │   │   │   │       ├── column_writer.h              # File: column writer
│   │   │   │   │   │   │       ├── interval_column_writer.h     # File: interval column writer
│   │   │   │   │   │   │       ├── list_column_writer.h         # File: list column writer
│   │   │   │   │   │   │       ├── parquet_rle_bp_encoder.h     # File: parquet rle bp encoder
│   │   │   │   │   │   │       ├── parquet_writer.h             # File: parquet writer
│   │   │   │   │   │   │       ├── standard_column_writer.h     # File: standard column writer
│   │   │   │   │   │   │       ├── string_column_writer.h       # File: string column writer
│   │   │   │   │   │   │       ├── struct_column_writer.h       # File: struct column writer
│   │   │   │   │   │   │       └── uuid_column_writer.h         # File: uuid column writer
│   │   │   │   │   │   ├── batch_insert.h                       # File: batch insert
│   │   │   │   │   │   ├── batch_insert_error_handler.h         # File: batch insert error handler
│   │   │   │   │   │   ├── copy_rel_batch_insert.h              # File: copy rel batch insert
│   │   │   │   │   │   ├── copy_to.h                            # File: copy to
│   │   │   │   │   │   ├── delete.h                             # File: delete
│   │   │   │   │   │   ├── delete_executor.h                    # File: delete executor
│   │   │   │   │   │   ├── index_builder.h                      # File: index builder
│   │   │   │   │   │   ├── insert.h                             # File: insert
│   │   │   │   │   │   ├── insert_executor.h                    # File: insert executor
│   │   │   │   │   │   ├── merge.h                              # File: merge
│   │   │   │   │   │   ├── node_batch_insert.h                  # File: node batch insert
│   │   │   │   │   │   ├── node_batch_insert_error_handler.h    # File: node batch insert error handler
│   │   │   │   │   │   ├── rel_batch_insert.h                   # File: rel batch insert
│   │   │   │   │   │   ├── set.h                                # File: set
│   │   │   │   │   │   └── set_executor.h                       # File: set executor
│   │   │   │   │   ├── scan/                                    # Scan
│   │   │   │   │   │   ├── count_rel_table.h                    # File: count rel table
│   │   │   │   │   │   ├── primary_key_scan_node_table.h        # File: primary key scan node table
│   │   │   │   │   │   ├── reachable_count.h                    # File: reachable count
│   │   │   │   │   │   ├── rel_degree_table.h                   # File: rel degree table
│   │   │   │   │   │   ├── scan_multi_rel_tables.h              # File: scan multi rel tables
│   │   │   │   │   │   ├── scan_node_table.h                    # File: scan node table
│   │   │   │   │   │   ├── scan_rel_table.h                     # File: scan rel table
│   │   │   │   │   │   └── scan_table.h                         # File: scan table
│   │   │   │   │   ├── simple/                                  # Simple
│   │   │   │   │   │   ├── analyze.h                            # File: analyze
│   │   │   │   │   │   ├── attach_database.h                    # File: attach database
│   │   │   │   │   │   ├── detach_database.h                    # File: detach database
│   │   │   │   │   │   ├── export_db.h                          # File: export db
│   │   │   │   │   │   ├── extension_print_info.h               # File: extension print info
│   │   │   │   │   │   ├── import_db.h                          # File: import db
│   │   │   │   │   │   ├── install_extension.h                  # File: install extension
│   │   │   │   │   │   ├── load_extension.h                     # File: load extension
│   │   │   │   │   │   ├── uninstall_extension.h                # File: uninstall extension
│   │   │   │   │   │   ├── use_database.h                       # File: use database
│   │   │   │   │   │   └── use_graph.h                          # File: use graph
│   │   │   │   │   ├── table_scan/                              # Table scan
│   │   │   │   │   │   ├── ftable_scan_function.h               # File: ftable scan function
│   │   │   │   │   │   └── union_all_scan.h                     # File: union all scan
│   │   │   │   │   ├── arrow_result_collector.h                 # File: arrow result collector
│   │   │   │   │   ├── base_partitioner_shared_state.h          # File: base partitioner shared state
│   │   │   │   │   ├── cross_product.h                          # File: cross product
│   │   │   │   │   ├── empty_result.h                           # File: empty result
│   │   │   │   │   ├── filter.h                                 # File: filter
│   │   │   │   │   ├── filtering_operator.h                     # File: filtering operator
│   │   │   │   │   ├── flatten.h                                # File: flatten
│   │   │   │   │   ├── index_lookup.h                           # File: index lookup
│   │   │   │   │   ├── limit.h                                  # File: limit
│   │   │   │   │   ├── multiplicity_reducer.h                   # File: multiplicity reducer
│   │   │   │   │   ├── partitioner.h                            # File: partitioner
│   │   │   │   │   ├── path_property_probe.h                    # File: path property probe
│   │   │   │   │   ├── physical_operator.h                      # File: physical operator
│   │   │   │   │   ├── profile.h                                # File: profile
│   │   │   │   │   ├── projection.h                             # File: projection
│   │   │   │   │   ├── query_primary_key_lookup.h               # File: query primary key lookup
│   │   │   │   │   ├── recursive_extend.h                       # File: recursive extend
│   │   │   │   │   ├── recursive_extend_shared_state.h          # File: recursive extend shared state
│   │   │   │   │   ├── result_collector.h                       # File: result collector
│   │   │   │   │   ├── semi_masker.h                            # File: semi masker
│   │   │   │   │   ├── sink.h                                   # File: sink
│   │   │   │   │   ├── skip.h                                   # File: skip
│   │   │   │   │   ├── standalone_call.h                        # File: standalone call
│   │   │   │   │   ├── table_function_call.h                    # File: table function call
│   │   │   │   │   ├── transaction.h                            # File: transaction
│   │   │   │   │   ├── unwind.h                                 # File: unwind
│   │   │   │   │   └── unwind_dedup.h                           # File: unwind dedup
│   │   │   │   ├── result/                                      # Result
│   │   │   │   │   ├── base_hash_table.h                        # File: base hash table
│   │   │   │   │   ├── factorized_table.h                       # File: factorized table
│   │   │   │   │   ├── factorized_table_pool.h                  # File: factorized table pool
│   │   │   │   │   ├── factorized_table_schema.h                # File: factorized table schema
│   │   │   │   │   ├── factorized_table_util.h                  # File: factorized table util
│   │   │   │   │   ├── flat_tuple.h                             # File: flat tuple
│   │   │   │   │   ├── pattern_creation_info_table.h            # File: pattern creation info table
│   │   │   │   │   ├── result_set.h                             # File: result set
│   │   │   │   │   └── result_set_descriptor.h                  # File: result set descriptor
│   │   │   │   ├── data_pos.h                                   # File: data pos
│   │   │   │   ├── execution_context.h                          # File: execution context
│   │   │   │   ├── expression_mapper.h                          # File: expression mapper
│   │   │   │   ├── partition_routing.h                          # File: partition routing
│   │   │   │   ├── physical_plan.h                              # File: physical plan
│   │   │   │   ├── physical_plan_util.h                         # File: physical plan util
│   │   │   │   ├── plan_mapper.h                                # File: plan mapper
│   │   │   │   ├── processor.h                                  # File: processor
│   │   │   │   ├── processor_task.h                             # File: processor task
│   │   │   │   └── warning_context.h                            # File: warning context
│   │   │   ├── storage/                                         # Storage
│   │   │   │   ├── buffer_manager/                              # Buffer manager
│   │   │   │   │   ├── buffer_manager.h                         # File: buffer manager
│   │   │   │   │   ├── memory_manager.h                         # File: memory manager
│   │   │   │   │   ├── mm_allocator.h                           # File: mm allocator
│   │   │   │   │   ├── page_state.h                             # File: page state
│   │   │   │   │   ├── spill_result.h                           # File: spill result
│   │   │   │   │   ├── spiller.h                                # File: spiller
│   │   │   │   │   └── vm_region.h                              # File: vm region
│   │   │   │   ├── compression/                                 # Compression
│   │   │   │   │   ├── bitpacking_int128.h                      # File: bitpacking int128
│   │   │   │   │   ├── bitpacking_utils.h                       # File: bitpacking utils
│   │   │   │   │   ├── compression.h                            # File: compression
│   │   │   │   │   ├── float_compression.h                      # File: float compression
│   │   │   │   │   └── sign_extend.h                            # File: sign extend
│   │   │   │   ├── enums/                                       # Enums
│   │   │   │   │   ├── csr_node_group_scan_source.h             # File: csr node group scan source
│   │   │   │   │   ├── page_read_policy.h                       # File: page read policy
│   │   │   │   │   └── residency_state.h                        # File: residency state
│   │   │   │   ├── index/                                       # Index
│   │   │   │   │   ├── art_index.h                              # File: art index
│   │   │   │   │   ├── art_index_disk_utils.h                   # File: art index disk utils
│   │   │   │   │   ├── hash_index.h                             # File: hash index
│   │   │   │   │   ├── hash_index_header.h                      # File: hash index header
│   │   │   │   │   ├── hash_index_slot.h                        # File: hash index slot
│   │   │   │   │   ├── hash_index_utils.h                       # File: hash index utils
│   │   │   │   │   ├── in_mem_hash_index.h                      # File: in mem hash index
│   │   │   │   │   └── index.h                                  # File: index
│   │   │   │   ├── local_storage/                               # Local storage
│   │   │   │   │   ├── local_hash_index.h                       # File: local hash index
│   │   │   │   │   ├── local_node_table.h                       # File: local node table
│   │   │   │   │   ├── local_rel_table.h                        # File: local rel table
│   │   │   │   │   ├── local_storage.h                          # File: local storage
│   │   │   │   │   └── local_table.h                            # File: local table
│   │   │   │   ├── predicate/                                   # Predicate
│   │   │   │   │   ├── column_predicate.h                       # File: column predicate
│   │   │   │   │   ├── constant_predicate.h                     # File: constant predicate
│   │   │   │   │   └── null_predicate.h                         # File: null predicate
│   │   │   │   ├── stats/                                       # Stats
│   │   │   │   │   ├── column_stats.h                           # File: column stats
│   │   │   │   │   ├── hyperloglog.h                            # File: hyperloglog
│   │   │   │   │   ├── planner_stats.h                          # File: planner stats
│   │   │   │   │   └── table_stats.h                            # File: table stats
│   │   │   │   ├── table/                                       # Table
│   │   │   │   │   ├── arrow_node_table.h                       # File: arrow node table
│   │   │   │   │   ├── arrow_rel_table.h                        # File: arrow rel table
│   │   │   │   │   ├── arrow_table_support.h                    # File: arrow table support
│   │   │   │   │   ├── chunked_node_group.h                     # File: chunked node group
│   │   │   │   │   ├── column.h                                 # File: column
│   │   │   │   │   ├── column_chunk.h                           # File: column chunk
│   │   │   │   │   ├── column_chunk_data.h                      # File: column chunk data
│   │   │   │   │   ├── column_chunk_metadata.h                  # File: column chunk metadata
│   │   │   │   │   ├── column_chunk_scanner.h                   # File: column chunk scanner
│   │   │   │   │   ├── column_chunk_stats.h                     # File: column chunk stats
│   │   │   │   │   ├── column_reader_writer.h                   # File: column reader writer
│   │   │   │   │   ├── columnar_node_table_base.h               # File: columnar node table base
│   │   │   │   │   ├── columnar_rel_table_base.h                # File: columnar rel table base
│   │   │   │   │   ├── combined_chunk_scanner.h                 # File: combined chunk scanner
│   │   │   │   │   ├── compression_flush_buffer.h               # File: compression flush buffer
│   │   │   │   │   ├── csr_chunked_node_group.h                 # File: csr chunked node group
│   │   │   │   │   ├── csr_node_group.h                         # File: csr node group
│   │   │   │   │   ├── dictionary_chunk.h                       # File: dictionary chunk
│   │   │   │   │   ├── dictionary_column.h                      # File: dictionary column
│   │   │   │   │   ├── foreign_rel_table.h                      # File: foreign rel table
│   │   │   │   │   ├── group_collection.h                       # File: group collection
│   │   │   │   │   ├── ice_disk_constants.h                     # File: ice disk constants
│   │   │   │   │   ├── ice_disk_node_table.h                    # File: ice disk node table
│   │   │   │   │   ├── ice_disk_rel_table.h                     # File: ice disk rel table
│   │   │   │   │   ├── ice_disk_utils.h                         # File: ice disk utils
│   │   │   │   │   ├── in_mem_chunked_node_group_collection.h   # File: in mem chunked node group collection
│   │   │   │   │   ├── in_memory_exception_chunk.h              # File: in memory exception chunk
│   │   │   │   │   ├── lazy_segment_scanner.h                   # File: lazy segment scanner
│   │   │   │   │   ├── list_chunk_data.h                        # File: list chunk data
│   │   │   │   │   ├── list_column.h                            # File: list column
│   │   │   │   │   ├── node_group.h                             # File: node group
│   │   │   │   │   ├── node_group_collection.h                  # File: node group collection
│   │   │   │   │   ├── node_table.h                             # File: node table
│   │   │   │   │   ├── null_column.h                            # File: null column
│   │   │   │   │   ├── rel_table.h                              # File: rel table
│   │   │   │   │   ├── rel_table_data.h                         # File: rel table data
│   │   │   │   │   ├── string_chunk_data.h                      # File: string chunk data
│   │   │   │   │   ├── string_column.h                          # File: string column
│   │   │   │   │   ├── struct_chunk_data.h                      # File: struct chunk data
│   │   │   │   │   ├── struct_column.h                          # File: struct column
│   │   │   │   │   ├── table.h                                  # File: table
│   │   │   │   │   ├── update_info.h                            # File: update info
│   │   │   │   │   ├── version_info.h                           # File: version info
│   │   │   │   │   └── version_record_handler.h                 # File: version record handler
│   │   │   │   ├── wal/                                         # Wal
│   │   │   │   │   ├── record/                                  # Record
│   │   │   │   │   │   ├── alter_table_entry_record.h           # File: alter table entry record
│   │   │   │   │   │   ├── begin_transaction_record.h           # File: begin transaction record
│   │   │   │   │   │   ├── checkpoint_record.h                  # File: checkpoint record
│   │   │   │   │   │   ├── commit_record.h                      # File: commit record
│   │   │   │   │   │   ├── copy_table_record.h                  # File: copy table record
│   │   │   │   │   │   ├── create_catalog_entry_record.h        # File: create catalog entry record
│   │   │   │   │   │   ├── create_index_record.h                # File: create index record
│   │   │   │   │   │   ├── drop_catalog_entry_record.h          # File: drop catalog entry record
│   │   │   │   │   │   ├── load_extension_record.h              # File: load extension record
│   │   │   │   │   │   ├── node_deletion_record.h               # File: node deletion record
│   │   │   │   │   │   ├── node_update_record.h                 # File: node update record
│   │   │   │   │   │   ├── rel_deletion_record.h                # File: rel deletion record
│   │   │   │   │   │   ├── rel_detach_delete_record.h           # File: rel detach delete record
│   │   │   │   │   │   ├── rel_update_record.h                  # File: rel update record
│   │   │   │   │   │   ├── table_insertion_record.h             # File: table insertion record
│   │   │   │   │   │   ├── update_sequence_record.h             # File: update sequence record
│   │   │   │   │   │   └── wal_record_base.h                    # File: wal record base
│   │   │   │   │   ├── checksum_reader.h                        # File: checksum reader
│   │   │   │   │   ├── checksum_writer.h                        # File: checksum writer
│   │   │   │   │   ├── local_wal.h                              # File: local wal
│   │   │   │   │   ├── wal.h                                    # File: wal
│   │   │   │   │   ├── wal_record.h                             # File: wal record
│   │   │   │   │   └── wal_replayer.h                           # File: wal replayer
│   │   │   │   ├── checkpointer.h                               # File: checkpointer
│   │   │   │   ├── database_header.h                            # File: database header
│   │   │   │   ├── disk_array.h                                 # File: disk array
│   │   │   │   ├── disk_array_collection.h                      # File: disk array collection
│   │   │   │   ├── file_db_id_utils.h                           # File: file db id utils
│   │   │   │   ├── file_handle.h                                # File: file handle
│   │   │   │   ├── free_space_manager.h                         # File: free space manager
│   │   │   │   ├── local_cached_column.h                        # File: local cached column
│   │   │   │   ├── optimistic_allocator.h                       # File: optimistic allocator
│   │   │   │   ├── overflow_file.h                              # File: overflow file
│   │   │   │   ├── page_allocator.h                             # File: page allocator
│   │   │   │   ├── page_manager.h                               # File: page manager
│   │   │   │   ├── page_range.h                                 # File: page range
│   │   │   │   ├── partition_storage_registry.h                 # File: partition storage registry
│   │   │   │   ├── shadow_file.h                                # File: shadow file
│   │   │   │   ├── shadow_utils.h                               # File: shadow utils
│   │   │   │   ├── storage_extension.h                          # File: storage extension
│   │   │   │   ├── storage_manager.h                            # File: storage manager
│   │   │   │   ├── storage_utils.h                              # File: storage utils
│   │   │   │   ├── storage_version_info.h                       # File: storage version info
│   │   │   │   └── undo_buffer.h                                # File: undo buffer
│   │   │   └── transaction/                                     # Transaction
│   │   │       ├── transaction.h                                # File: transaction
│   │   │       ├── transaction_action.h                         # File: transaction action
│   │   │       ├── transaction_context.h                        # File: transaction context
│   │   │       └── transaction_manager.h                        # File: transaction manager
│   │   ├── main/                                                # Main
│   │   │   ├── query_result/                                    # Query result
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── arrow_query_result.cpp                       # File: arrow query result
│   │   │   │   └── materialized_query_result.cpp                # File: materialized query result
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── attached_database.cpp                            # File: attached database
│   │   │   ├── client_context.cpp                               # File: client context
│   │   │   ├── connection.cpp                                   # File: connection
│   │   │   ├── database.cpp                                     # File: database
│   │   │   ├── database_manager.cpp                             # File: database manager
│   │   │   ├── db_config.cpp                                    # File: db config
│   │   │   ├── plan_printer.cpp                                 # File: plan printer
│   │   │   ├── prepared_statement.cpp                           # File: prepared statement
│   │   │   ├── prepared_statement_manager.cpp                   # File: prepared statement manager
│   │   │   ├── query_result.cpp                                 # File: query result
│   │   │   ├── query_summary.cpp                                # File: query summary
│   │   │   ├── settings.cpp                                     # File: settings
│   │   │   ├── storage_driver.cpp                               # File: storage driver
│   │   │   └── version.cpp                                      # File: version
│   │   ├── optimizer/                                           # Optimizer
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── acc_hash_join_optimizer.cpp                      # File: acc hash join optimizer
│   │   │   ├── agg_key_dependency_optimizer.cpp                 # File: agg key dependency optimizer
│   │   │   ├── bool_folding_optimizer.cpp                       # File: bool folding optimizer
│   │   │   ├── cardinality_updater.cpp                          # File: cardinality updater
│   │   │   ├── correlated_subquery_unnest_solver.cpp            # File: correlated subquery unnest solver
│   │   │   ├── count_rel_table_optimizer.cpp                    # File: count rel table optimizer
│   │   │   ├── factorization_rewriter.cpp                       # File: factorization rewriter
│   │   │   ├── filter_push_down_optimizer.cpp                   # File: filter push down optimizer
│   │   │   ├── foreign_join_push_down_optimizer.cpp             # File: foreign join push down optimizer
│   │   │   ├── limit_push_down_optimizer.cpp                    # File: limit push down optimizer
│   │   │   ├── logical_operator_collector.cpp                   # File: logical operator collector
│   │   │   ├── logical_operator_visitor.cpp                     # File: logical operator visitor
│   │   │   ├── optimizer.cpp                                    # File: optimizer
│   │   │   ├── order_by_push_down_optimizer.cpp                 # File: order by push down optimizer
│   │   │   ├── projection_push_down_optimizer.cpp               # File: projection push down optimizer
│   │   │   ├── remove_factorization_rewriter.cpp                # File: remove factorization rewriter
│   │   │   ├── remove_unnecessary_distinct_optimizer.cpp        # File: remove unnecessary distinct optimizer
│   │   │   ├── remove_unnecessary_join_optimizer.cpp            # File: remove unnecessary join optimizer
│   │   │   ├── remove_unnecessary_order_by_optimizer.cpp        # File: remove unnecessary order by optimizer
│   │   │   ├── schema_populator.cpp                             # File: schema populator
│   │   │   ├── top_k_optimizer.cpp                              # File: top k optimizer
│   │   │   └── unwind_dedup_optimizer.cpp                       # File: unwind dedup optimizer
│   │   ├── parser/                                              # Parser
│   │   │   ├── antlr_parser/                                    # Antlr parser
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── lbug_cypher_parser.cpp                       # File: lbug cypher parser
│   │   │   │   ├── parser_error_listener.cpp                    # File: parser error listener
│   │   │   │   └── parser_error_strategy.cpp                    # File: parser error strategy
│   │   │   ├── expression/                                      # Expression
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── parsed_case_expression.cpp                   # File: parsed case expression
│   │   │   │   ├── parsed_expression.cpp                        # File: parsed expression
│   │   │   │   ├── parsed_expression_visitor.cpp                # File: parsed expression visitor
│   │   │   │   ├── parsed_function_expression.cpp               # File: parsed function expression
│   │   │   │   ├── parsed_property_expression.cpp               # File: parsed property expression
│   │   │   │   └── parsed_variable_expression.cpp               # File: parsed variable expression
│   │   │   ├── transform/                                       # Transform
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── transform_attach_database.cpp                # File: transform attach database
│   │   │   │   ├── transform_copy.cpp                           # File: transform copy
│   │   │   │   ├── transform_ddl.cpp                            # File: transform ddl
│   │   │   │   ├── transform_detach_database.cpp                # File: transform detach database
│   │   │   │   ├── transform_expression.cpp                     # File: transform expression
│   │   │   │   ├── transform_extension.cpp                      # File: transform extension
│   │   │   │   ├── transform_graph.cpp                          # File: transform graph
│   │   │   │   ├── transform_graph_pattern.cpp                  # File: transform graph pattern
│   │   │   │   ├── transform_macro.cpp                          # File: transform macro
│   │   │   │   ├── transform_port_db.cpp                        # File: transform port db
│   │   │   │   ├── transform_projection.cpp                     # File: transform projection
│   │   │   │   ├── transform_query.cpp                          # File: transform query
│   │   │   │   ├── transform_reading_clause.cpp                 # File: transform reading clause
│   │   │   │   ├── transform_standalone_call.cpp                # File: transform standalone call
│   │   │   │   ├── transform_transaction.cpp                    # File: transform transaction
│   │   │   │   ├── transform_updating_clause.cpp                # File: transform updating clause
│   │   │   │   └── transform_use_database.cpp                   # File: transform use database
│   │   │   ├── visitor/                                         # Visitor
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── standalone_call_rewriter.cpp                 # File: standalone call rewriter
│   │   │   │   └── statement_read_write_analyzer.cpp            # File: statement read write analyzer
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── create_macro.cpp                                 # File: create macro
│   │   │   ├── parsed_statement_visitor.cpp                     # File: parsed statement visitor
│   │   │   ├── parser.cpp                                       # File: parser
│   │   │   └── transformer.cpp                                  # File: transformer
│   │   ├── planner/                                             # Planner
│   │   │   ├── join_order/                                      # Join order
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── cardinality_estimator.cpp                    # File: cardinality estimator
│   │   │   │   ├── cost_model.cpp                               # File: cost model
│   │   │   │   ├── join_order_util.cpp                          # File: join order util
│   │   │   │   ├── join_plan_solver.cpp                         # File: join plan solver
│   │   │   │   ├── join_tree.cpp                                # File: join tree
│   │   │   │   └── join_tree_constructor.cpp                    # File: join tree constructor
│   │   │   ├── operator/                                        # Operator
│   │   │   │   ├── extend/                                      # Extend
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── base_logical_extend.cpp                  # File: base logical extend
│   │   │   │   │   ├── logical_extend.cpp                       # File: logical extend
│   │   │   │   │   ├── logical_packed_extend.cpp                # File: logical packed extend
│   │   │   │   │   └── logical_recursive_extend.cpp             # File: logical recursive extend
│   │   │   │   ├── factorization/                               # Factorization
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── flatten_resolver.cpp                     # File: flatten resolver
│   │   │   │   │   └── sink_util.cpp                            # File: sink util
│   │   │   │   ├── persistent/                                  # Persistent
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── logical_copy_from.cpp                    # File: logical copy from
│   │   │   │   │   ├── logical_copy_to.cpp                      # File: logical copy to
│   │   │   │   │   ├── logical_delete.cpp                       # File: logical delete
│   │   │   │   │   ├── logical_insert.cpp                       # File: logical insert
│   │   │   │   │   ├── logical_merge.cpp                        # File: logical merge
│   │   │   │   │   └── logical_set.cpp                          # File: logical set
│   │   │   │   ├── scan/                                        # Scan
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── logical_count_rel_table.cpp              # File: logical count rel table
│   │   │   │   │   ├── logical_expressions_scan.cpp             # File: logical expressions scan
│   │   │   │   │   ├── logical_index_look_up.cpp                # File: logical index look up
│   │   │   │   │   ├── logical_query_primary_key_lookup.cpp     # File: logical query primary key lookup
│   │   │   │   │   ├── logical_reachable_count.cpp              # File: logical reachable count
│   │   │   │   │   ├── logical_rel_degree_table.cpp             # File: logical rel degree table
│   │   │   │   │   └── logical_scan_node_table.cpp              # File: logical scan node table
│   │   │   │   ├── simple/                                      # Simple
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   └── logical_simple.cpp                       # File: logical simple
│   │   │   │   ├── sip/                                         # Sip
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   └── logical_semi_masker.cpp                  # File: logical semi masker
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── logical_accumulate.cpp                       # File: logical accumulate
│   │   │   │   ├── logical_aggregate.cpp                        # File: logical aggregate
│   │   │   │   ├── logical_create_macro.cpp                     # File: logical create macro
│   │   │   │   ├── logical_cross_product.cpp                    # File: logical cross product
│   │   │   │   ├── logical_distinct.cpp                         # File: logical distinct
│   │   │   │   ├── logical_dummy_scan.cpp                       # File: logical dummy scan
│   │   │   │   ├── logical_dummy_sink.cpp                       # File: logical dummy sink
│   │   │   │   ├── logical_explain.cpp                          # File: logical explain
│   │   │   │   ├── logical_filter.cpp                           # File: logical filter
│   │   │   │   ├── logical_flatten.cpp                          # File: logical flatten
│   │   │   │   ├── logical_hash_join.cpp                        # File: logical hash join
│   │   │   │   ├── logical_intersect.cpp                        # File: logical intersect
│   │   │   │   ├── logical_limit.cpp                            # File: logical limit
│   │   │   │   ├── logical_operator.cpp                         # File: logical operator
│   │   │   │   ├── logical_order_by.cpp                         # File: logical order by
│   │   │   │   ├── logical_partitioner.cpp                      # File: logical partitioner
│   │   │   │   ├── logical_path_property_probe.cpp              # File: logical path property probe
│   │   │   │   ├── logical_plan.cpp                             # File: logical plan
│   │   │   │   ├── logical_plan_util.cpp                        # File: logical plan util
│   │   │   │   ├── logical_projection.cpp                       # File: logical projection
│   │   │   │   ├── logical_standalone_call.cpp                  # File: logical standalone call
│   │   │   │   ├── logical_table_function_call.cpp              # File: logical table function call
│   │   │   │   ├── logical_union.cpp                            # File: logical union
│   │   │   │   ├── logical_unwind.cpp                           # File: logical unwind
│   │   │   │   ├── logical_unwind_deduplicate.cpp               # File: logical unwind deduplicate
│   │   │   │   └── schema.cpp                                   # File: schema
│   │   │   ├── plan/                                            # Plan
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── append_accumulate.cpp                        # File: append accumulate
│   │   │   │   ├── append_aggregate.cpp                         # File: append aggregate
│   │   │   │   ├── append_cross_product.cpp                     # File: append cross product
│   │   │   │   ├── append_delete.cpp                            # File: append delete
│   │   │   │   ├── append_distinct.cpp                          # File: append distinct
│   │   │   │   ├── append_dummy_scan.cpp                        # File: append dummy scan
│   │   │   │   ├── append_empty_result.cpp                      # File: append empty result
│   │   │   │   ├── append_expressions_scan.cpp                  # File: append expressions scan
│   │   │   │   ├── append_extend.cpp                            # File: append extend
│   │   │   │   ├── append_filter.cpp                            # File: append filter
│   │   │   │   ├── append_flatten.cpp                           # File: append flatten
│   │   │   │   ├── append_insert.cpp                            # File: append insert
│   │   │   │   ├── append_join.cpp                              # File: append join
│   │   │   │   ├── append_limit.cpp                             # File: append limit
│   │   │   │   ├── append_multiplicity_reducer.cpp              # File: append multiplicity reducer
│   │   │   │   ├── append_order_by.cpp                          # File: append order by
│   │   │   │   ├── append_projection.cpp                        # File: append projection
│   │   │   │   ├── append_scan_node_table.cpp                   # File: append scan node table
│   │   │   │   ├── append_set.cpp                               # File: append set
│   │   │   │   ├── append_simple.cpp                            # File: append simple
│   │   │   │   ├── append_table_function_call.cpp               # File: append table function call
│   │   │   │   ├── append_unwind.cpp                            # File: append unwind
│   │   │   │   ├── plan_copy.cpp                                # File: plan copy
│   │   │   │   ├── plan_join_order.cpp                          # File: plan join order
│   │   │   │   ├── plan_node_scan.cpp                           # File: plan node scan
│   │   │   │   ├── plan_node_semi_mask.cpp                      # File: plan node semi mask
│   │   │   │   ├── plan_port_db.cpp                             # File: plan port db
│   │   │   │   ├── plan_projection.cpp                          # File: plan projection
│   │   │   │   ├── plan_read.cpp                                # File: plan read
│   │   │   │   ├── plan_single_query.cpp                        # File: plan single query
│   │   │   │   ├── plan_subquery.cpp                            # File: plan subquery
│   │   │   │   └── plan_update.cpp                              # File: plan update
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── join_order_enumerator_context.cpp                # File: join order enumerator context
│   │   │   ├── planner.cpp                                      # File: planner
│   │   │   ├── query_planner.cpp                                # File: query planner
│   │   │   └── subplans_table.cpp                               # File: subplans table
│   │   ├── processor/                                           # Processor
│   │   │   ├── map/                                             # Map
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── create_arrow_result_collector.cpp            # File: create arrow result collector
│   │   │   │   ├── create_factorized_table_scan.cpp             # File: create factorized table scan
│   │   │   │   ├── create_result_collector.cpp                  # File: create result collector
│   │   │   │   ├── expression_mapper.cpp                        # File: expression mapper
│   │   │   │   ├── map_acc_hash_join.cpp                        # File: map acc hash join
│   │   │   │   ├── map_accumulate.cpp                           # File: map accumulate
│   │   │   │   ├── map_aggregate.cpp                            # File: map aggregate
│   │   │   │   ├── map_analyze.cpp                              # File: map analyze
│   │   │   │   ├── map_copy_from.cpp                            # File: map copy from
│   │   │   │   ├── map_copy_to.cpp                              # File: map copy to
│   │   │   │   ├── map_count_rel_table.cpp                      # File: map count rel table
│   │   │   │   ├── map_create_macro.cpp                         # File: map create macro
│   │   │   │   ├── map_cross_product.cpp                        # File: map cross product
│   │   │   │   ├── map_ddl.cpp                                  # File: map ddl
│   │   │   │   ├── map_delete.cpp                               # File: map delete
│   │   │   │   ├── map_distinct.cpp                             # File: map distinct
│   │   │   │   ├── map_dummy_scan.cpp                           # File: map dummy scan
│   │   │   │   ├── map_dummy_sink.cpp                           # File: map dummy sink
│   │   │   │   ├── map_empty_result.cpp                         # File: map empty result
│   │   │   │   ├── map_explain.cpp                              # File: map explain
│   │   │   │   ├── map_expressions_scan.cpp                     # File: map expressions scan
│   │   │   │   ├── map_extend.cpp                               # File: map extend
│   │   │   │   ├── map_filter.cpp                               # File: map filter
│   │   │   │   ├── map_flatten.cpp                              # File: map flatten
│   │   │   │   ├── map_hash_join.cpp                            # File: map hash join
│   │   │   │   ├── map_index_scan_node.cpp                      # File: map index scan node
│   │   │   │   ├── map_insert.cpp                               # File: map insert
│   │   │   │   ├── map_intersect.cpp                            # File: map intersect
│   │   │   │   ├── map_label_filter.cpp                         # File: map label filter
│   │   │   │   ├── map_limit.cpp                                # File: map limit
│   │   │   │   ├── map_merge.cpp                                # File: map merge
│   │   │   │   ├── map_multiplicity_reducer.cpp                 # File: map multiplicity reducer
│   │   │   │   ├── map_noop.cpp                                 # File: map noop
│   │   │   │   ├── map_order_by.cpp                             # File: map order by
│   │   │   │   ├── map_path_property_probe.cpp                  # File: map path property probe
│   │   │   │   ├── map_projection.cpp                           # File: map projection
│   │   │   │   ├── map_query_primary_key_lookup.cpp             # File: map query primary key lookup
│   │   │   │   ├── map_reachable_count.cpp                      # File: map reachable count
│   │   │   │   ├── map_recursive_extend.cpp                     # File: map recursive extend
│   │   │   │   ├── map_rel_degree_table.cpp                     # File: map rel degree table
│   │   │   │   ├── map_scan_node_table.cpp                      # File: map scan node table
│   │   │   │   ├── map_semi_masker.cpp                          # File: map semi masker
│   │   │   │   ├── map_set.cpp                                  # File: map set
│   │   │   │   ├── map_simple.cpp                               # File: map simple
│   │   │   │   ├── map_standalone_call.cpp                      # File: map standalone call
│   │   │   │   ├── map_table_function_call.cpp                  # File: map table function call
│   │   │   │   ├── map_transaction.cpp                          # File: map transaction
│   │   │   │   ├── map_union.cpp                                # File: map union
│   │   │   │   ├── map_unwind.cpp                               # File: map unwind
│   │   │   │   ├── map_unwind_dedup.cpp                         # File: map unwind dedup
│   │   │   │   └── plan_mapper.cpp                              # File: plan mapper
│   │   │   ├── operator/                                        # Operator
│   │   │   │   ├── aggregate/                                   # Aggregate
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── aggregate_hash_table.cpp                 # File: aggregate hash table
│   │   │   │   │   ├── base_aggregate.cpp                       # File: base aggregate
│   │   │   │   │   ├── base_aggregate_scan.cpp                  # File: base aggregate scan
│   │   │   │   │   ├── hash_aggregate.cpp                       # File: hash aggregate
│   │   │   │   │   ├── hash_aggregate_scan.cpp                  # File: hash aggregate scan
│   │   │   │   │   ├── packed_filtered_count.cpp                # File: packed filtered count
│   │   │   │   │   ├── simple_aggregate.cpp                     # File: simple aggregate
│   │   │   │   │   └── simple_aggregate_scan.cpp                # File: simple aggregate scan
│   │   │   │   ├── ddl/                                         # Ddl
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── alter.cpp                                # File: alter
│   │   │   │   │   ├── create_index.cpp                         # File: create index
│   │   │   │   │   ├── create_sequence.cpp                      # File: create sequence
│   │   │   │   │   ├── create_table.cpp                         # File: create table
│   │   │   │   │   ├── create_type.cpp                          # File: create type
│   │   │   │   │   └── drop.cpp                                 # File: drop
│   │   │   │   ├── hash_join/                                   # Hash join
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── hash_join_build.cpp                      # File: hash join build
│   │   │   │   │   ├── hash_join_probe.cpp                      # File: hash join probe
│   │   │   │   │   └── join_hash_table.cpp                      # File: join hash table
│   │   │   │   ├── intersect/                                   # Intersect
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   └── intersect.cpp                            # File: intersect
│   │   │   │   ├── macro/                                       # Macro
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   └── create_macro.cpp                         # File: create macro
│   │   │   │   ├── order_by/                                    # Order by
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── key_block_merger.cpp                     # File: key block merger
│   │   │   │   │   ├── order_by.cpp                             # File: order by
│   │   │   │   │   ├── order_by_key_encoder.cpp                 # File: order by key encoder
│   │   │   │   │   ├── order_by_merge.cpp                       # File: order by merge
│   │   │   │   │   ├── order_by_scan.cpp                        # File: order by scan
│   │   │   │   │   ├── radix_sort.cpp                           # File: radix sort
│   │   │   │   │   ├── sort_state.cpp                           # File: sort state
│   │   │   │   │   ├── top_k.cpp                                # File: top k
│   │   │   │   │   └── top_k_scanner.cpp                        # File: top k scanner
│   │   │   │   ├── persistent/                                  # Persistent
│   │   │   │   │   ├── reader/                                  # Reader
│   │   │   │   │   │   ├── csv/                                 # Csv
│   │   │   │   │   │   │   ├── CMakeLists.txt                   # Text: CMakeLists
│   │   │   │   │   │   │   ├── base_csv_reader.cpp              # File: base csv reader
│   │   │   │   │   │   │   ├── csv_boundary_scanner.cpp         # File: csv boundary scanner
│   │   │   │   │   │   │   ├── dialect_detection.cpp            # File: dialect detection
│   │   │   │   │   │   │   ├── driver.cpp                       # File: driver
│   │   │   │   │   │   │   ├── parallel_csv_reader.cpp          # File: parallel csv reader
│   │   │   │   │   │   │   └── serial_csv_reader.cpp            # File: serial csv reader
│   │   │   │   │   │   ├── npy/                                 # Npy
│   │   │   │   │   │   │   ├── CMakeLists.txt                   # Text: CMakeLists
│   │   │   │   │   │   │   └── npy_reader.cpp                   # File: npy reader
│   │   │   │   │   │   ├── parquet/                             # Parquet
│   │   │   │   │   │   │   ├── CMakeLists.txt                   # Text: CMakeLists
│   │   │   │   │   │   │   ├── boolean_column_reader.cpp        # File: boolean column reader
│   │   │   │   │   │   │   ├── column_reader.cpp                # File: column reader
│   │   │   │   │   │   │   ├── interval_column_reader.cpp       # File: interval column reader
│   │   │   │   │   │   │   ├── list_column_reader.cpp           # File: list column reader
│   │   │   │   │   │   │   ├── parquet_reader.cpp               # File: parquet reader
│   │   │   │   │   │   │   ├── parquet_timestamp.cpp            # File: parquet timestamp
│   │   │   │   │   │   │   ├── string_column_reader.cpp         # File: string column reader
│   │   │   │   │   │   │   ├── struct_column_reader.cpp         # File: struct column reader
│   │   │   │   │   │   │   └── uuid_column_reader.cpp           # File: uuid column reader
│   │   │   │   │   │   ├── CMakeLists.txt                       # Text: CMakeLists
│   │   │   │   │   │   ├── copy_from_error.cpp                  # File: copy from error
│   │   │   │   │   │   ├── file_error_handler.cpp               # File: file error handler
│   │   │   │   │   │   └── reader_bind_utils.cpp                # File: reader bind utils
│   │   │   │   │   ├── writer/                                  # Writer
│   │   │   │   │   │   └── parquet/                             # Parquet
│   │   │   │   │   │       ├── CMakeLists.txt                   # Text: CMakeLists
│   │   │   │   │   │       ├── basic_column_writer.cpp          # File: basic column writer
│   │   │   │   │   │       ├── boolean_column_writer.cpp        # File: boolean column writer
│   │   │   │   │   │       ├── column_writer.cpp                # File: column writer
│   │   │   │   │   │       ├── interval_column_writer.cpp       # File: interval column writer
│   │   │   │   │   │       ├── list_column_writer.cpp           # File: list column writer
│   │   │   │   │   │       ├── parquet_rle_bp_encoder.cpp       # File: parquet rle bp encoder
│   │   │   │   │   │       ├── parquet_writer.cpp               # File: parquet writer
│   │   │   │   │   │       ├── string_column_writer.cpp         # File: string column writer
│   │   │   │   │   │       ├── struct_column_writer.cpp         # File: struct column writer
│   │   │   │   │   │       └── uuid_column_writer.cpp           # File: uuid column writer
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── batch_insert_error_handler.cpp           # File: batch insert error handler
│   │   │   │   │   ├── copy_rel_batch_insert.cpp                # File: copy rel batch insert
│   │   │   │   │   ├── copy_to.cpp                              # File: copy to
│   │   │   │   │   ├── delete.cpp                               # File: delete
│   │   │   │   │   ├── delete_executor.cpp                      # File: delete executor
│   │   │   │   │   ├── index_builder.cpp                        # File: index builder
│   │   │   │   │   ├── insert.cpp                               # File: insert
│   │   │   │   │   ├── insert_executor.cpp                      # File: insert executor
│   │   │   │   │   ├── merge.cpp                                # File: merge
│   │   │   │   │   ├── node_batch_insert.cpp                    # File: node batch insert
│   │   │   │   │   ├── node_batch_insert_error_handler.cpp      # File: node batch insert error handler
│   │   │   │   │   ├── rel_batch_insert.cpp                     # File: rel batch insert
│   │   │   │   │   ├── set.cpp                                  # File: set
│   │   │   │   │   └── set_executor.cpp                         # File: set executor
│   │   │   │   ├── scan/                                        # Scan
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── count_rel_table.cpp                      # File: count rel table
│   │   │   │   │   ├── primary_key_scan_node_table.cpp          # File: primary key scan node table
│   │   │   │   │   ├── reachable_count.cpp                      # File: reachable count
│   │   │   │   │   ├── rel_degree_table.cpp                     # File: rel degree table
│   │   │   │   │   ├── scan_multi_rel_tables.cpp                # File: scan multi rel tables
│   │   │   │   │   ├── scan_node_table.cpp                      # File: scan node table
│   │   │   │   │   ├── scan_rel_table.cpp                       # File: scan rel table
│   │   │   │   │   └── scan_table.cpp                           # File: scan table
│   │   │   │   ├── simple/                                      # Simple
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── analyze.cpp                              # File: analyze
│   │   │   │   │   ├── attach_database.cpp                      # File: attach database
│   │   │   │   │   ├── detach_database.cpp                      # File: detach database
│   │   │   │   │   ├── export_db.cpp                            # File: export db
│   │   │   │   │   ├── import_db.cpp                            # File: import db
│   │   │   │   │   ├── install_extension.cpp                    # File: install extension
│   │   │   │   │   ├── load_extension.cpp                       # File: load extension
│   │   │   │   │   ├── uninstall_extension.cpp                  # File: uninstall extension
│   │   │   │   │   ├── use_database.cpp                         # File: use database
│   │   │   │   │   └── use_graph.cpp                            # File: use graph
│   │   │   │   ├── table_scan/                                  # Table scan
│   │   │   │   │   ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │   │   ├── ftable_scan_function.cpp                 # File: ftable scan function
│   │   │   │   │   └── union_all_scan.cpp                       # File: union all scan
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── arrow_result_collector.cpp                   # File: arrow result collector
│   │   │   │   ├── base_partitioner_shared_state.cpp            # File: base partitioner shared state
│   │   │   │   ├── cross_product.cpp                            # File: cross product
│   │   │   │   ├── empty_result.cpp                             # File: empty result
│   │   │   │   ├── filter.cpp                                   # File: filter
│   │   │   │   ├── filtering_operator.cpp                       # File: filtering operator
│   │   │   │   ├── flatten.cpp                                  # File: flatten
│   │   │   │   ├── index_lookup.cpp                             # File: index lookup
│   │   │   │   ├── limit.cpp                                    # File: limit
│   │   │   │   ├── multiplicity_reducer.cpp                     # File: multiplicity reducer
│   │   │   │   ├── partitioner.cpp                              # File: partitioner
│   │   │   │   ├── path_property_probe.cpp                      # File: path property probe
│   │   │   │   ├── physical_operator.cpp                        # File: physical operator
│   │   │   │   ├── profile.cpp                                  # File: profile
│   │   │   │   ├── projection.cpp                               # File: projection
│   │   │   │   ├── query_primary_key_lookup.cpp                 # File: query primary key lookup
│   │   │   │   ├── recursive_extend.cpp                         # File: recursive extend
│   │   │   │   ├── result_collector.cpp                         # File: result collector
│   │   │   │   ├── semi_masker.cpp                              # File: semi masker
│   │   │   │   ├── sink.cpp                                     # File: sink
│   │   │   │   ├── skip.cpp                                     # File: skip
│   │   │   │   ├── standalone_call.cpp                          # File: standalone call
│   │   │   │   ├── table_function_call.cpp                      # File: table function call
│   │   │   │   ├── transaction.cpp                              # File: transaction
│   │   │   │   ├── unwind.cpp                                   # File: unwind
│   │   │   │   └── unwind_dedup.cpp                             # File: unwind dedup
│   │   │   ├── result/                                          # Result
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── base_hash_table.cpp                          # File: base hash table
│   │   │   │   ├── factorized_table.cpp                         # File: factorized table
│   │   │   │   ├── factorized_table_pool.cpp                    # File: factorized table pool
│   │   │   │   ├── factorized_table_schema.cpp                  # File: factorized table schema
│   │   │   │   ├── factorized_table_util.cpp                    # File: factorized table util
│   │   │   │   ├── flat_tuple.cpp                               # File: flat tuple
│   │   │   │   ├── pattern_creation_info_table.cpp              # File: pattern creation info table
│   │   │   │   ├── result_set.cpp                               # File: result set
│   │   │   │   └── result_set_descriptor.cpp                    # File: result set descriptor
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── partition_routing.cpp                            # File: partition routing
│   │   │   ├── physical_plan_util.cpp                           # File: physical plan util
│   │   │   ├── processor.cpp                                    # File: processor
│   │   │   ├── processor_task.cpp                               # File: processor task
│   │   │   └── warning_context.cpp                              # File: warning context
│   │   ├── storage/                                             # Storage
│   │   │   ├── buffer_manager/                                  # Buffer manager
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── buffer_manager.cpp                           # File: buffer manager
│   │   │   │   ├── memory_manager.cpp                           # File: memory manager
│   │   │   │   ├── spiller.cpp                                  # File: spiller
│   │   │   │   └── vm_region.cpp                                # File: vm region
│   │   │   ├── compression/                                     # Compression
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── bitpacking_int128.cpp                        # File: bitpacking int128
│   │   │   │   ├── bitpacking_utils.cpp                         # File: bitpacking utils
│   │   │   │   ├── compression.cpp                              # File: compression
│   │   │   │   └── float_compression.cpp                        # File: float compression
│   │   │   ├── index/                                           # Index
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── art_index.cpp                                # File: art index
│   │   │   │   ├── art_index_disk.cpp                           # File: art index disk
│   │   │   │   ├── hash_index.cpp                               # File: hash index
│   │   │   │   ├── in_mem_hash_index.cpp                        # File: in mem hash index
│   │   │   │   └── index.cpp                                    # File: index
│   │   │   ├── local_storage/                                   # Local storage
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── local_node_table.cpp                         # File: local node table
│   │   │   │   ├── local_rel_table.cpp                          # File: local rel table
│   │   │   │   └── local_storage.cpp                            # File: local storage
│   │   │   ├── predicate/                                       # Predicate
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── column_predicate.cpp                         # File: column predicate
│   │   │   │   ├── constant_predicate.cpp                       # File: constant predicate
│   │   │   │   └── null_predicate.cpp                           # File: null predicate
│   │   │   ├── stats/                                           # Stats
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── column_stats.cpp                             # File: column stats
│   │   │   │   ├── hyperloglog.cpp                              # File: hyperloglog
│   │   │   │   ├── planner_stats.cpp                            # File: planner stats
│   │   │   │   └── table_stats.cpp                              # File: table stats
│   │   │   ├── table/                                           # Table
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── arrow_node_table.cpp                         # File: arrow node table
│   │   │   │   ├── arrow_rel_table.cpp                          # File: arrow rel table
│   │   │   │   ├── arrow_table_support.cpp                      # File: arrow table support
│   │   │   │   ├── chunked_node_group.cpp                       # File: chunked node group
│   │   │   │   ├── column.cpp                                   # File: column
│   │   │   │   ├── column_chunk.cpp                             # File: column chunk
│   │   │   │   ├── column_chunk_data.cpp                        # File: column chunk data
│   │   │   │   ├── column_chunk_metadata.cpp                    # File: column chunk metadata
│   │   │   │   ├── column_chunk_stats.cpp                       # File: column chunk stats
│   │   │   │   ├── column_reader_writer.cpp                     # File: column reader writer
│   │   │   │   ├── columnar_node_table_base.cpp                 # File: columnar node table base
│   │   │   │   ├── columnar_rel_table_base.cpp                  # File: columnar rel table base
│   │   │   │   ├── compression_flush_buffer.cpp                 # File: compression flush buffer
│   │   │   │   ├── csr_chunked_node_group.cpp                   # File: csr chunked node group
│   │   │   │   ├── csr_node_group.cpp                           # File: csr node group
│   │   │   │   ├── dictionary_chunk.cpp                         # File: dictionary chunk
│   │   │   │   ├── dictionary_column.cpp                        # File: dictionary column
│   │   │   │   ├── foreign_rel_table.cpp                        # File: foreign rel table
│   │   │   │   ├── ice_disk_node_table.cpp                      # File: ice disk node table
│   │   │   │   ├── ice_disk_rel_table.cpp                       # File: ice disk rel table
│   │   │   │   ├── in_mem_chunked_node_group_collection.cpp     # File: in mem chunked node group collection
│   │   │   │   ├── in_memory_exception_chunk.cpp                # File: in memory exception chunk
│   │   │   │   ├── lazy_segment_scanner.cpp                     # File: lazy segment scanner
│   │   │   │   ├── list_chunk_data.cpp                          # File: list chunk data
│   │   │   │   ├── list_column.cpp                              # File: list column
│   │   │   │   ├── node_group.cpp                               # File: node group
│   │   │   │   ├── node_group_collection.cpp                    # File: node group collection
│   │   │   │   ├── node_table.cpp                               # File: node table
│   │   │   │   ├── null_column.cpp                              # File: null column
│   │   │   │   ├── rel_table.cpp                                # File: rel table
│   │   │   │   ├── rel_table_data.cpp                           # File: rel table data
│   │   │   │   ├── string_chunk_data.cpp                        # File: string chunk data
│   │   │   │   ├── string_column.cpp                            # File: string column
│   │   │   │   ├── struct_chunk_data.cpp                        # File: struct chunk data
│   │   │   │   ├── struct_column.cpp                            # File: struct column
│   │   │   │   ├── table.cpp                                    # File: table
│   │   │   │   ├── update_info.cpp                              # File: update info
│   │   │   │   ├── version_info.cpp                             # File: version info
│   │   │   │   └── version_record_handler.cpp                   # File: version record handler
│   │   │   ├── wal/                                             # Wal
│   │   │   │   ├── records/                                     # Records
│   │   │   │   │   ├── alter_table_entry_record.cpp             # File: alter table entry record
│   │   │   │   │   ├── alter_table_entry_record_replay.cpp      # File: alter table entry record replay
│   │   │   │   │   ├── begin_transaction_record.cpp             # File: begin transaction record
│   │   │   │   │   ├── checkpoint_record.cpp                    # File: checkpoint record
│   │   │   │   │   ├── commit_record.cpp                        # File: commit record
│   │   │   │   │   ├── copy_table_record.cpp                    # File: copy table record
│   │   │   │   │   ├── copy_table_record_replay.cpp             # File: copy table record replay
│   │   │   │   │   ├── create_catalog_entry_record.cpp          # File: create catalog entry record
│   │   │   │   │   ├── create_catalog_entry_record_replay.cpp   # File: create catalog entry record replay
│   │   │   │   │   ├── create_index_record.cpp                  # File: create index record
│   │   │   │   │   ├── create_index_record_replay.cpp           # File: create index record replay
│   │   │   │   │   ├── drop_catalog_entry_record.cpp            # File: drop catalog entry record
│   │   │   │   │   ├── drop_catalog_entry_record_replay.cpp     # File: drop catalog entry record replay
│   │   │   │   │   ├── load_extension_record.cpp                # File: load extension record
│   │   │   │   │   ├── load_extension_record_replay.cpp         # File: load extension record replay
│   │   │   │   │   ├── node_deletion_record.cpp                 # File: node deletion record
│   │   │   │   │   ├── node_deletion_record_replay.cpp          # File: node deletion record replay
│   │   │   │   │   ├── node_update_record.cpp                   # File: node update record
│   │   │   │   │   ├── node_update_record_replay.cpp            # File: node update record replay
│   │   │   │   │   ├── rel_deletion_record.cpp                  # File: rel deletion record
│   │   │   │   │   ├── rel_deletion_record_replay.cpp           # File: rel deletion record replay
│   │   │   │   │   ├── rel_detach_delete_record.cpp             # File: rel detach delete record
│   │   │   │   │   ├── rel_detach_delete_record_replay.cpp      # File: rel detach delete record replay
│   │   │   │   │   ├── rel_update_record.cpp                    # File: rel update record
│   │   │   │   │   ├── rel_update_record_replay.cpp             # File: rel update record replay
│   │   │   │   │   ├── table_insertion_record.cpp               # File: table insertion record
│   │   │   │   │   ├── table_insertion_record_replay.cpp        # File: table insertion record replay
│   │   │   │   │   ├── update_sequence_record.cpp               # File: update sequence record
│   │   │   │   │   └── update_sequence_record_replay.cpp        # File: update sequence record replay
│   │   │   │   ├── typespec/                                    # WAL record declarations live in records/*.tsp and are generated with
│   │   │   │   │   ├── records/                                 # Records
│   │   │   │   │   │   ├── alter_table_entry_record.tsp         # File: alter table entry record
│   │   │   │   │   │   ├── begin_transaction_record.tsp         # File: begin transaction record
│   │   │   │   │   │   ├── checkpoint_record.tsp                # File: checkpoint record
│   │   │   │   │   │   ├── commit_record.tsp                    # File: commit record
│   │   │   │   │   │   ├── copy_table_record.tsp                # File: copy table record
│   │   │   │   │   │   ├── create_catalog_entry_record.tsp      # File: create catalog entry record
│   │   │   │   │   │   ├── create_index_record.tsp              # File: create index record
│   │   │   │   │   │   ├── drop_catalog_entry_record.tsp        # File: drop catalog entry record
│   │   │   │   │   │   ├── load_extension_record.tsp            # File: load extension record
│   │   │   │   │   │   ├── node_deletion_record.tsp             # File: node deletion record
│   │   │   │   │   │   ├── node_update_record.tsp               # File: node update record
│   │   │   │   │   │   ├── rel_deletion_record.tsp              # File: rel deletion record
│   │   │   │   │   │   ├── rel_detach_delete_record.tsp         # File: rel detach delete record
│   │   │   │   │   │   ├── rel_update_record.tsp                # File: rel update record
│   │   │   │   │   │   ├── table_insertion_record.tsp           # File: table insertion record
│   │   │   │   │   │   └── update_sequence_record.tsp           # File: update sequence record
│   │   │   │   │   ├── templates/                               # Templates
│   │   │   │   │   │   ├── wal_record_header.h.j2               # File: wal record header.h
│   │   │   │   │   │   └── wal_record_source.cpp.j2             # File: wal record source.cpp
│   │   │   │   │   ├── README.md                                # WAL record declarations live in records/*.tsp and are generated with
│   │   │   │   │   └── common.tsp                               # File: common
│   │   │   │   ├── CMakeLists.txt                               # Text: CMakeLists
│   │   │   │   ├── checksum_reader.cpp                          # File: checksum reader
│   │   │   │   ├── checksum_writer.cpp                          # File: checksum writer
│   │   │   │   ├── local_wal.cpp                                # File: local wal
│   │   │   │   ├── wal.cpp                                      # File: wal
│   │   │   │   ├── wal_record.cpp                               # File: wal record
│   │   │   │   └── wal_replayer.cpp                             # File: wal replayer
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── checkpointer.cpp                                 # File: checkpointer
│   │   │   ├── database_header.cpp                              # File: database header
│   │   │   ├── disk_array.cpp                                   # File: disk array
│   │   │   ├── disk_array_collection.cpp                        # File: disk array collection
│   │   │   ├── file_db_id_utils.cpp                             # File: file db id utils
│   │   │   ├── file_handle.cpp                                  # File: file handle
│   │   │   ├── free_space_manager.cpp                           # File: free space manager
│   │   │   ├── optimistic_allocator.cpp                         # File: optimistic allocator
│   │   │   ├── overflow_file.cpp                                # File: overflow file
│   │   │   ├── page_manager.cpp                                 # File: page manager
│   │   │   ├── partition_storage_registry.cpp                   # File: partition storage registry
│   │   │   ├── shadow_file.cpp                                  # File: shadow file
│   │   │   ├── shadow_utils.cpp                                 # File: shadow utils
│   │   │   ├── storage_manager.cpp                              # File: storage manager
│   │   │   ├── storage_utils.cpp                                # File: storage utils
│   │   │   ├── storage_version_info.cpp                         # File: storage version info
│   │   │   └── undo_buffer.cpp                                  # File: undo buffer
│   │   ├── transaction/                                         # Transaction
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── transaction.cpp                                  # File: transaction
│   │   │   ├── transaction_context.cpp                          # File: transaction context
│   │   │   └── transaction_manager.cpp                          # File: transaction manager
│   │   └── CMakeLists.txt                                       # Text: CMakeLists
│   ├── third_party/                                             # Third party
│   │   ├── alp/                                                 # Alp
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── alp/                                         # Alp
│   │   │   │   │   ├── common.hpp                               # File: common
│   │   │   │   │   ├── config.hpp                               # File: config
│   │   │   │   │   ├── constants.hpp                            # File: constants
│   │   │   │   │   ├── decode.hpp                               # File: decode
│   │   │   │   │   ├── encode.hpp                               # File: encode
│   │   │   │   │   ├── sampler.hpp                              # File: sampler
│   │   │   │   │   ├── state.hpp                                # File: state
│   │   │   │   │   ├── storer.hpp                               # File: storer
│   │   │   │   │   └── utils.hpp                                # File: utils
│   │   │   │   └── alp.hpp                                      # File: alp
│   │   │   ├── .clang-format                                    # File: clang format
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── antlr4_cypher/                                       # Antlr4 cypher
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── cypher_lexer.h                               # File: cypher lexer
│   │   │   │   └── cypher_parser.h                              # File: cypher parser
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── cypher_lexer.cpp                                 # File: cypher lexer
│   │   │   └── cypher_parser.cpp                                # File: cypher parser
│   │   ├── antlr4_runtime/                                      # Antlr4 runtime
│   │   │   ├── src/                                             # The crate's sources
│   │   │   │   ├── atn/                                         # Atn
│   │   │   │   │   ├── ATN.cpp                                  # File: ATN
│   │   │   │   │   ├── ATN.h                                    # File: ATN
│   │   │   │   │   ├── ATNConfig.cpp                            # File: ATNConfig
│   │   │   │   │   ├── ATNConfig.h                              # File: ATNConfig
│   │   │   │   │   ├── ATNConfigSet.cpp                         # File: ATNConfigSet
│   │   │   │   │   ├── ATNConfigSet.h                           # File: ATNConfigSet
│   │   │   │   │   ├── ATNDeserializationOptions.cpp            # File: ATNDeserializationOptions
│   │   │   │   │   ├── ATNDeserializationOptions.h              # File: ATNDeserializationOptions
│   │   │   │   │   ├── ATNDeserializer.cpp                      # File: ATNDeserializer
│   │   │   │   │   ├── ATNDeserializer.h                        # File: ATNDeserializer
│   │   │   │   │   ├── ATNSimulator.cpp                         # File: ATNSimulator
│   │   │   │   │   ├── ATNSimulator.h                           # File: ATNSimulator
│   │   │   │   │   ├── ATNState.cpp                             # File: ATNState
│   │   │   │   │   ├── ATNState.h                               # File: ATNState
│   │   │   │   │   ├── ATNStateType.cpp                         # File: ATNStateType
│   │   │   │   │   ├── ATNStateType.h                           # File: ATNStateType
│   │   │   │   │   ├── ATNType.h                                # File: ATNType
│   │   │   │   │   ├── ActionTransition.cpp                     # File: ActionTransition
│   │   │   │   │   ├── ActionTransition.h                       # File: ActionTransition
│   │   │   │   │   ├── AmbiguityInfo.cpp                        # File: AmbiguityInfo
│   │   │   │   │   ├── AmbiguityInfo.h                          # File: AmbiguityInfo
│   │   │   │   │   ├── ArrayPredictionContext.cpp               # File: ArrayPredictionContext
│   │   │   │   │   ├── ArrayPredictionContext.h                 # File: ArrayPredictionContext
│   │   │   │   │   ├── AtomTransition.cpp                       # File: AtomTransition
│   │   │   │   │   ├── AtomTransition.h                         # File: AtomTransition
│   │   │   │   │   ├── BasicBlockStartState.h                   # File: BasicBlockStartState
│   │   │   │   │   ├── BasicState.h                             # File: BasicState
│   │   │   │   │   ├── BlockEndState.h                          # File: BlockEndState
│   │   │   │   │   ├── BlockStartState.h                        # File: BlockStartState
│   │   │   │   │   ├── ContextSensitivityInfo.cpp               # File: ContextSensitivityInfo
│   │   │   │   │   ├── ContextSensitivityInfo.h                 # File: ContextSensitivityInfo
│   │   │   │   │   ├── DecisionEventInfo.cpp                    # File: DecisionEventInfo
│   │   │   │   │   ├── DecisionEventInfo.h                      # File: DecisionEventInfo
│   │   │   │   │   ├── DecisionInfo.cpp                         # File: DecisionInfo
│   │   │   │   │   ├── DecisionInfo.h                           # File: DecisionInfo
│   │   │   │   │   ├── DecisionState.cpp                        # File: DecisionState
│   │   │   │   │   ├── DecisionState.h                          # File: DecisionState
│   │   │   │   │   ├── EpsilonTransition.cpp                    # File: EpsilonTransition
│   │   │   │   │   ├── EpsilonTransition.h                      # File: EpsilonTransition
│   │   │   │   │   ├── ErrorInfo.cpp                            # File: ErrorInfo
│   │   │   │   │   ├── ErrorInfo.h                              # File: ErrorInfo
│   │   │   │   │   ├── HashUtils.h                              # File: HashUtils
│   │   │   │   │   ├── LL1Analyzer.cpp                          # File: LL1Analyzer
│   │   │   │   │   ├── LL1Analyzer.h                            # File: LL1Analyzer
│   │   │   │   │   ├── LexerATNConfig.cpp                       # File: LexerATNConfig
│   │   │   │   │   ├── LexerATNConfig.h                         # File: LexerATNConfig
│   │   │   │   │   ├── LexerATNSimulator.cpp                    # File: LexerATNSimulator
│   │   │   │   │   ├── LexerATNSimulator.h                      # File: LexerATNSimulator
│   │   │   │   │   ├── LexerAction.cpp                          # File: LexerAction
│   │   │   │   │   ├── LexerAction.h                            # File: LexerAction
│   │   │   │   │   ├── LexerActionExecutor.cpp                  # File: LexerActionExecutor
│   │   │   │   │   ├── LexerActionExecutor.h                    # File: LexerActionExecutor
│   │   │   │   │   ├── LexerActionType.h                        # File: LexerActionType
│   │   │   │   │   ├── LexerChannelAction.cpp                   # File: LexerChannelAction
│   │   │   │   │   ├── LexerChannelAction.h                     # File: LexerChannelAction
│   │   │   │   │   ├── LexerCustomAction.cpp                    # File: LexerCustomAction
│   │   │   │   │   ├── LexerCustomAction.h                      # File: LexerCustomAction
│   │   │   │   │   ├── LexerIndexedCustomAction.cpp             # File: LexerIndexedCustomAction
│   │   │   │   │   ├── LexerIndexedCustomAction.h               # File: LexerIndexedCustomAction
│   │   │   │   │   ├── LexerModeAction.cpp                      # File: LexerModeAction
│   │   │   │   │   ├── LexerModeAction.h                        # File: LexerModeAction
│   │   │   │   │   ├── LexerMoreAction.cpp                      # File: LexerMoreAction
│   │   │   │   │   ├── LexerMoreAction.h                        # File: LexerMoreAction
│   │   │   │   │   ├── LexerPopModeAction.cpp                   # File: LexerPopModeAction
│   │   │   │   │   ├── LexerPopModeAction.h                     # File: LexerPopModeAction
│   │   │   │   │   ├── LexerPushModeAction.cpp                  # File: LexerPushModeAction
│   │   │   │   │   ├── LexerPushModeAction.h                    # File: LexerPushModeAction
│   │   │   │   │   ├── LexerSkipAction.cpp                      # File: LexerSkipAction
│   │   │   │   │   ├── LexerSkipAction.h                        # File: LexerSkipAction
│   │   │   │   │   ├── LexerTypeAction.cpp                      # File: LexerTypeAction
│   │   │   │   │   ├── LexerTypeAction.h                        # File: LexerTypeAction
│   │   │   │   │   ├── LookaheadEventInfo.cpp                   # File: LookaheadEventInfo
│   │   │   │   │   ├── LookaheadEventInfo.h                     # File: LookaheadEventInfo
│   │   │   │   │   ├── LoopEndState.h                           # File: LoopEndState
│   │   │   │   │   ├── NotSetTransition.cpp                     # File: NotSetTransition
│   │   │   │   │   ├── NotSetTransition.h                       # File: NotSetTransition
│   │   │   │   │   ├── OrderedATNConfigSet.cpp                  # File: OrderedATNConfigSet
│   │   │   │   │   ├── OrderedATNConfigSet.h                    # File: OrderedATNConfigSet
│   │   │   │   │   ├── ParseInfo.cpp                            # File: ParseInfo
│   │   │   │   │   ├── ParseInfo.h                              # File: ParseInfo
│   │   │   │   │   ├── ParserATNSimulator.cpp                   # File: ParserATNSimulator
│   │   │   │   │   ├── ParserATNSimulator.h                     # File: ParserATNSimulator
│   │   │   │   │   ├── ParserATNSimulatorOptions.h              # File: ParserATNSimulatorOptions
│   │   │   │   │   ├── PlusBlockStartState.h                    # File: PlusBlockStartState
│   │   │   │   │   ├── PlusLoopbackState.h                      # File: PlusLoopbackState
│   │   │   │   │   ├── PrecedencePredicateTransition.cpp        # File: PrecedencePredicateTransition
│   │   │   │   │   ├── PrecedencePredicateTransition.h          # File: PrecedencePredicateTransition
│   │   │   │   │   ├── PredicateEvalInfo.cpp                    # File: PredicateEvalInfo
│   │   │   │   │   ├── PredicateEvalInfo.h                      # File: PredicateEvalInfo
│   │   │   │   │   ├── PredicateTransition.cpp                  # File: PredicateTransition
│   │   │   │   │   ├── PredicateTransition.h                    # File: PredicateTransition
│   │   │   │   │   ├── PredictionContext.cpp                    # File: PredictionContext
│   │   │   │   │   ├── PredictionContext.h                      # File: PredictionContext
│   │   │   │   │   ├── PredictionContextCache.cpp               # File: PredictionContextCache
│   │   │   │   │   ├── PredictionContextCache.h                 # File: PredictionContextCache
│   │   │   │   │   ├── PredictionContextMergeCache.cpp          # File: PredictionContextMergeCache
│   │   │   │   │   ├── PredictionContextMergeCache.h            # File: PredictionContextMergeCache
│   │   │   │   │   ├── PredictionContextMergeCacheOptions.h     # File: PredictionContextMergeCacheOptions
│   │   │   │   │   ├── PredictionContextType.h                  # File: PredictionContextType
│   │   │   │   │   ├── PredictionMode.cpp                       # File: PredictionMode
│   │   │   │   │   ├── PredictionMode.h                         # File: PredictionMode
│   │   │   │   │   ├── ProfilingATNSimulator.cpp                # File: ProfilingATNSimulator
│   │   │   │   │   ├── ProfilingATNSimulator.h                  # File: ProfilingATNSimulator
│   │   │   │   │   ├── RangeTransition.cpp                      # File: RangeTransition
│   │   │   │   │   ├── RangeTransition.h                        # File: RangeTransition
│   │   │   │   │   ├── RuleStartState.h                         # File: RuleStartState
│   │   │   │   │   ├── RuleStopState.h                          # File: RuleStopState
│   │   │   │   │   ├── RuleTransition.cpp                       # File: RuleTransition
│   │   │   │   │   ├── RuleTransition.h                         # File: RuleTransition
│   │   │   │   │   ├── SemanticContext.cpp                      # File: SemanticContext
│   │   │   │   │   ├── SemanticContext.h                        # File: SemanticContext
│   │   │   │   │   ├── SemanticContextType.h                    # File: SemanticContextType
│   │   │   │   │   ├── SerializedATNView.h                      # File: SerializedATNView
│   │   │   │   │   ├── SetTransition.cpp                        # File: SetTransition
│   │   │   │   │   ├── SetTransition.h                          # File: SetTransition
│   │   │   │   │   ├── SingletonPredictionContext.cpp           # File: SingletonPredictionContext
│   │   │   │   │   ├── SingletonPredictionContext.h             # File: SingletonPredictionContext
│   │   │   │   │   ├── StarBlockStartState.h                    # File: StarBlockStartState
│   │   │   │   │   ├── StarLoopEntryState.h                     # File: StarLoopEntryState
│   │   │   │   │   ├── StarLoopbackState.cpp                    # File: StarLoopbackState
│   │   │   │   │   ├── StarLoopbackState.h                      # File: StarLoopbackState
│   │   │   │   │   ├── TokensStartState.h                       # File: TokensStartState
│   │   │   │   │   ├── Transition.cpp                           # File: Transition
│   │   │   │   │   ├── Transition.h                             # File: Transition
│   │   │   │   │   ├── TransitionType.cpp                       # File: TransitionType
│   │   │   │   │   ├── TransitionType.h                         # File: TransitionType
│   │   │   │   │   ├── WildcardTransition.cpp                   # File: WildcardTransition
│   │   │   │   │   └── WildcardTransition.h                     # File: WildcardTransition
│   │   │   │   ├── dfa/                                         # Dfa
│   │   │   │   │   ├── DFA.cpp                                  # File: DFA
│   │   │   │   │   ├── DFA.h                                    # File: DFA
│   │   │   │   │   ├── DFASerializer.cpp                        # File: DFASerializer
│   │   │   │   │   ├── DFASerializer.h                          # File: DFASerializer
│   │   │   │   │   ├── DFAState.cpp                             # File: DFAState
│   │   │   │   │   ├── DFAState.h                               # File: DFAState
│   │   │   │   │   ├── LexerDFASerializer.cpp                   # File: LexerDFASerializer
│   │   │   │   │   └── LexerDFASerializer.h                     # File: LexerDFASerializer
│   │   │   │   ├── internal/                                    # Internal
│   │   │   │   │   ├── Synchronization.cpp                      # File: Synchronization
│   │   │   │   │   └── Synchronization.h                        # File: Synchronization
│   │   │   │   ├── misc/                                        # Misc
│   │   │   │   │   ├── InterpreterDataReader.cpp                # File: InterpreterDataReader
│   │   │   │   │   ├── InterpreterDataReader.h                  # File: InterpreterDataReader
│   │   │   │   │   ├── Interval.cpp                             # File: Interval
│   │   │   │   │   ├── Interval.h                               # File: Interval
│   │   │   │   │   ├── IntervalSet.cpp                          # File: IntervalSet
│   │   │   │   │   ├── IntervalSet.h                            # File: IntervalSet
│   │   │   │   │   ├── MurmurHash.cpp                           # File: MurmurHash
│   │   │   │   │   ├── MurmurHash.h                             # File: MurmurHash
│   │   │   │   │   ├── Predicate.cpp                            # File: Predicate
│   │   │   │   │   └── Predicate.h                              # File: Predicate
│   │   │   │   ├── support/                                     # Support
│   │   │   │   │   ├── Any.cpp                                  # File: Any
│   │   │   │   │   ├── Any.h                                    # File: Any
│   │   │   │   │   ├── Arrays.cpp                               # File: Arrays
│   │   │   │   │   ├── Arrays.h                                 # File: Arrays
│   │   │   │   │   ├── BitSet.h                                 # File: BitSet
│   │   │   │   │   ├── CPPUtils.cpp                             # File: CPPUtils
│   │   │   │   │   ├── CPPUtils.h                               # File: CPPUtils
│   │   │   │   │   ├── Casts.h                                  # File: Casts
│   │   │   │   │   ├── Declarations.h                           # File: Declarations
│   │   │   │   │   ├── StringUtils.cpp                          # File: StringUtils
│   │   │   │   │   ├── StringUtils.h                            # File: StringUtils
│   │   │   │   │   ├── Unicode.h                                # File: Unicode
│   │   │   │   │   ├── Utf8.cpp                                 # File: Utf8
│   │   │   │   │   └── Utf8.h                                   # File: Utf8
│   │   │   │   ├── tree/                                        # Tree
│   │   │   │   │   ├── pattern/                                 # Pattern
│   │   │   │   │   │   ├── Chunk.cpp                            # File: Chunk
│   │   │   │   │   │   ├── Chunk.h                              # File: Chunk
│   │   │   │   │   │   ├── ParseTreeMatch.cpp                   # File: ParseTreeMatch
│   │   │   │   │   │   ├── ParseTreeMatch.h                     # File: ParseTreeMatch
│   │   │   │   │   │   ├── ParseTreePattern.cpp                 # File: ParseTreePattern
│   │   │   │   │   │   ├── ParseTreePattern.h                   # File: ParseTreePattern
│   │   │   │   │   │   ├── ParseTreePatternMatcher.cpp          # File: ParseTreePatternMatcher
│   │   │   │   │   │   ├── ParseTreePatternMatcher.h            # File: ParseTreePatternMatcher
│   │   │   │   │   │   ├── RuleTagToken.cpp                     # File: RuleTagToken
│   │   │   │   │   │   ├── RuleTagToken.h                       # File: RuleTagToken
│   │   │   │   │   │   ├── TagChunk.cpp                         # File: TagChunk
│   │   │   │   │   │   ├── TagChunk.h                           # File: TagChunk
│   │   │   │   │   │   ├── TextChunk.cpp                        # File: TextChunk
│   │   │   │   │   │   ├── TextChunk.h                          # File: TextChunk
│   │   │   │   │   │   ├── TokenTagToken.cpp                    # File: TokenTagToken
│   │   │   │   │   │   └── TokenTagToken.h                      # File: TokenTagToken
│   │   │   │   │   ├── xpath/                                   # Xpath
│   │   │   │   │   │   ├── XPath.cpp                            # File: XPath
│   │   │   │   │   │   ├── XPath.h                              # File: XPath
│   │   │   │   │   │   ├── XPathElement.cpp                     # File: XPathElement
│   │   │   │   │   │   ├── XPathElement.h                       # File: XPathElement
│   │   │   │   │   │   ├── XPathLexer.cpp                       # File: XPathLexer
│   │   │   │   │   │   ├── XPathLexer.g4                        # File: XPathLexer
│   │   │   │   │   │   ├── XPathLexer.h                         # File: XPathLexer
│   │   │   │   │   │   ├── XPathLexer.tokens                    # File: XPathLexer
│   │   │   │   │   │   ├── XPathLexerErrorListener.cpp          # File: XPathLexerErrorListener
│   │   │   │   │   │   ├── XPathLexerErrorListener.h            # File: XPathLexerErrorListener
│   │   │   │   │   │   ├── XPathRuleAnywhereElement.cpp         # File: XPathRuleAnywhereElement
│   │   │   │   │   │   ├── XPathRuleAnywhereElement.h           # File: XPathRuleAnywhereElement
│   │   │   │   │   │   ├── XPathRuleElement.cpp                 # File: XPathRuleElement
│   │   │   │   │   │   ├── XPathRuleElement.h                   # File: XPathRuleElement
│   │   │   │   │   │   ├── XPathTokenAnywhereElement.cpp        # File: XPathTokenAnywhereElement
│   │   │   │   │   │   ├── XPathTokenAnywhereElement.h          # File: XPathTokenAnywhereElement
│   │   │   │   │   │   ├── XPathTokenElement.cpp                # File: XPathTokenElement
│   │   │   │   │   │   ├── XPathTokenElement.h                  # File: XPathTokenElement
│   │   │   │   │   │   ├── XPathWildcardAnywhereElement.cpp     # File: XPathWildcardAnywhereElement
│   │   │   │   │   │   ├── XPathWildcardAnywhereElement.h       # File: XPathWildcardAnywhereElement
│   │   │   │   │   │   ├── XPathWildcardElement.cpp             # File: XPathWildcardElement
│   │   │   │   │   │   └── XPathWildcardElement.h               # File: XPathWildcardElement
│   │   │   │   │   ├── AbstractParseTreeVisitor.h               # File: AbstractParseTreeVisitor
│   │   │   │   │   ├── ErrorNode.h                              # File: ErrorNode
│   │   │   │   │   ├── ErrorNodeImpl.cpp                        # File: ErrorNodeImpl
│   │   │   │   │   ├── ErrorNodeImpl.h                          # File: ErrorNodeImpl
│   │   │   │   │   ├── IterativeParseTreeWalker.cpp             # File: IterativeParseTreeWalker
│   │   │   │   │   ├── IterativeParseTreeWalker.h               # File: IterativeParseTreeWalker
│   │   │   │   │   ├── ParseTree.cpp                            # File: ParseTree
│   │   │   │   │   ├── ParseTree.h                              # File: ParseTree
│   │   │   │   │   ├── ParseTreeListener.cpp                    # File: ParseTreeListener
│   │   │   │   │   ├── ParseTreeListener.h                      # File: ParseTreeListener
│   │   │   │   │   ├── ParseTreeProperty.h                      # File: ParseTreeProperty
│   │   │   │   │   ├── ParseTreeType.h                          # File: ParseTreeType
│   │   │   │   │   ├── ParseTreeVisitor.cpp                     # File: ParseTreeVisitor
│   │   │   │   │   ├── ParseTreeVisitor.h                       # File: ParseTreeVisitor
│   │   │   │   │   ├── ParseTreeWalker.cpp                      # File: ParseTreeWalker
│   │   │   │   │   ├── ParseTreeWalker.h                        # File: ParseTreeWalker
│   │   │   │   │   ├── TerminalNode.h                           # File: TerminalNode
│   │   │   │   │   ├── TerminalNodeImpl.cpp                     # File: TerminalNodeImpl
│   │   │   │   │   ├── TerminalNodeImpl.h                       # File: TerminalNodeImpl
│   │   │   │   │   ├── Trees.cpp                                # File: Trees
│   │   │   │   │   └── Trees.h                                  # File: Trees
│   │   │   │   ├── ANTLRErrorListener.cpp                       # File: ANTLRErrorListener
│   │   │   │   ├── ANTLRErrorListener.h                         # File: ANTLRErrorListener
│   │   │   │   ├── ANTLRErrorStrategy.cpp                       # File: ANTLRErrorStrategy
│   │   │   │   ├── ANTLRErrorStrategy.h                         # File: ANTLRErrorStrategy
│   │   │   │   ├── ANTLRFileStream.cpp                          # File: ANTLRFileStream
│   │   │   │   ├── ANTLRFileStream.h                            # File: ANTLRFileStream
│   │   │   │   ├── ANTLRInputStream.cpp                         # File: ANTLRInputStream
│   │   │   │   ├── ANTLRInputStream.h                           # File: ANTLRInputStream
│   │   │   │   ├── BailErrorStrategy.cpp                        # File: BailErrorStrategy
│   │   │   │   ├── BailErrorStrategy.h                          # File: BailErrorStrategy
│   │   │   │   ├── BaseErrorListener.cpp                        # File: BaseErrorListener
│   │   │   │   ├── BaseErrorListener.h                          # File: BaseErrorListener
│   │   │   │   ├── BufferedTokenStream.cpp                      # File: BufferedTokenStream
│   │   │   │   ├── BufferedTokenStream.h                        # File: BufferedTokenStream
│   │   │   │   ├── CharStream.cpp                               # File: CharStream
│   │   │   │   ├── CharStream.h                                 # File: CharStream
│   │   │   │   ├── CommonToken.cpp                              # File: CommonToken
│   │   │   │   ├── CommonToken.h                                # File: CommonToken
│   │   │   │   ├── CommonTokenFactory.cpp                       # File: CommonTokenFactory
│   │   │   │   ├── CommonTokenFactory.h                         # File: CommonTokenFactory
│   │   │   │   ├── CommonTokenStream.cpp                        # File: CommonTokenStream
│   │   │   │   ├── CommonTokenStream.h                          # File: CommonTokenStream
│   │   │   │   ├── ConsoleErrorListener.cpp                     # File: ConsoleErrorListener
│   │   │   │   ├── ConsoleErrorListener.h                       # File: ConsoleErrorListener
│   │   │   │   ├── DefaultErrorStrategy.cpp                     # File: DefaultErrorStrategy
│   │   │   │   ├── DefaultErrorStrategy.h                       # File: DefaultErrorStrategy
│   │   │   │   ├── DiagnosticErrorListener.cpp                  # File: DiagnosticErrorListener
│   │   │   │   ├── DiagnosticErrorListener.h                    # File: DiagnosticErrorListener
│   │   │   │   ├── Exceptions.cpp                               # File: Exceptions
│   │   │   │   ├── Exceptions.h                                 # File: Exceptions
│   │   │   │   ├── FailedPredicateException.cpp                 # File: FailedPredicateException
│   │   │   │   ├── FailedPredicateException.h                   # File: FailedPredicateException
│   │   │   │   ├── FlatHashMap.h                                # File: FlatHashMap
│   │   │   │   ├── FlatHashSet.h                                # File: FlatHashSet
│   │   │   │   ├── InputMismatchException.cpp                   # File: InputMismatchException
│   │   │   │   ├── InputMismatchException.h                     # File: InputMismatchException
│   │   │   │   ├── IntStream.cpp                                # File: IntStream
│   │   │   │   ├── IntStream.h                                  # File: IntStream
│   │   │   │   ├── InterpreterRuleContext.cpp                   # File: InterpreterRuleContext
│   │   │   │   ├── InterpreterRuleContext.h                     # File: InterpreterRuleContext
│   │   │   │   ├── Lexer.cpp                                    # File: Lexer
│   │   │   │   ├── Lexer.h                                      # File: Lexer
│   │   │   │   ├── LexerInterpreter.cpp                         # File: LexerInterpreter
│   │   │   │   ├── LexerInterpreter.h                           # File: LexerInterpreter
│   │   │   │   ├── LexerNoViableAltException.cpp                # File: LexerNoViableAltException
│   │   │   │   ├── LexerNoViableAltException.h                  # File: LexerNoViableAltException
│   │   │   │   ├── ListTokenSource.cpp                          # File: ListTokenSource
│   │   │   │   ├── ListTokenSource.h                            # File: ListTokenSource
│   │   │   │   ├── NoViableAltException.cpp                     # File: NoViableAltException
│   │   │   │   ├── NoViableAltException.h                       # File: NoViableAltException
│   │   │   │   ├── Parser.cpp                                   # File: Parser
│   │   │   │   ├── Parser.h                                     # File: Parser
│   │   │   │   ├── ParserInterpreter.cpp                        # File: ParserInterpreter
│   │   │   │   ├── ParserInterpreter.h                          # File: ParserInterpreter
│   │   │   │   ├── ParserRuleContext.cpp                        # File: ParserRuleContext
│   │   │   │   ├── ParserRuleContext.h                          # File: ParserRuleContext
│   │   │   │   ├── ProxyErrorListener.cpp                       # File: ProxyErrorListener
│   │   │   │   ├── ProxyErrorListener.h                         # File: ProxyErrorListener
│   │   │   │   ├── RecognitionException.cpp                     # File: RecognitionException
│   │   │   │   ├── RecognitionException.h                       # File: RecognitionException
│   │   │   │   ├── Recognizer.cpp                               # File: Recognizer
│   │   │   │   ├── Recognizer.h                                 # File: Recognizer
│   │   │   │   ├── RuleContext.cpp                              # File: RuleContext
│   │   │   │   ├── RuleContext.h                                # File: RuleContext
│   │   │   │   ├── RuleContextWithAltNum.cpp                    # File: RuleContextWithAltNum
│   │   │   │   ├── RuleContextWithAltNum.h                      # File: RuleContextWithAltNum
│   │   │   │   ├── RuntimeMetaData.cpp                          # File: RuntimeMetaData
│   │   │   │   ├── RuntimeMetaData.h                            # File: RuntimeMetaData
│   │   │   │   ├── Token.cpp                                    # File: Token
│   │   │   │   ├── Token.h                                      # File: Token
│   │   │   │   ├── TokenFactory.h                               # File: TokenFactory
│   │   │   │   ├── TokenSource.cpp                              # File: TokenSource
│   │   │   │   ├── TokenSource.h                                # File: TokenSource
│   │   │   │   ├── TokenStream.cpp                              # File: TokenStream
│   │   │   │   ├── TokenStream.h                                # File: TokenStream
│   │   │   │   ├── TokenStreamRewriter.cpp                      # File: TokenStreamRewriter
│   │   │   │   ├── TokenStreamRewriter.h                        # File: TokenStreamRewriter
│   │   │   │   ├── UnbufferedCharStream.cpp                     # File: UnbufferedCharStream
│   │   │   │   ├── UnbufferedCharStream.h                       # File: UnbufferedCharStream
│   │   │   │   ├── UnbufferedTokenStream.cpp                    # File: UnbufferedTokenStream
│   │   │   │   ├── UnbufferedTokenStream.h                      # File: UnbufferedTokenStream
│   │   │   │   ├── Version.h                                    # File: Version
│   │   │   │   ├── Vocabulary.cpp                               # File: Vocabulary
│   │   │   │   ├── Vocabulary.h                                 # File: Vocabulary
│   │   │   │   ├── WritableToken.cpp                            # File: WritableToken
│   │   │   │   ├── WritableToken.h                              # File: WritableToken
│   │   │   │   ├── antlr4-common.h                              # File: antlr4 common
│   │   │   │   └── antlr4-runtime.h                             # File: antlr4 runtime
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── brotli/                                              # Brotli is a generic-purpose lossless compression algorithm that compresses data using a combination of a modern variant of the LZ77
│   │   │   ├── c/                                               # C
│   │   │   │   ├── common/                                      # Common
│   │   │   │   │   ├── constants.c                              # File: constants
│   │   │   │   │   ├── constants.h                              # File: constants
│   │   │   │   │   ├── context.c                                # File: context
│   │   │   │   │   ├── context.h                                # File: context
│   │   │   │   │   ├── dictionary.bin                           # File: dictionary
│   │   │   │   │   ├── dictionary.bin.br                        # File: dictionary.bin
│   │   │   │   │   ├── dictionary.c                             # File: dictionary
│   │   │   │   │   ├── dictionary.h                             # File: dictionary
│   │   │   │   │   ├── platform.c                               # File: platform
│   │   │   │   │   ├── platform.h                               # File: platform
│   │   │   │   │   ├── shared_dictionary.c                      # File: shared dictionary
│   │   │   │   │   ├── shared_dictionary_internal.h             # File: shared dictionary internal
│   │   │   │   │   ├── transform.c                              # File: transform
│   │   │   │   │   ├── transform.h                              # File: transform
│   │   │   │   │   └── version.h                                # File: version
│   │   │   │   ├── dec/                                         # Dec
│   │   │   │   │   ├── bit_reader.c                             # File: bit reader
│   │   │   │   │   ├── bit_reader.h                             # File: bit reader
│   │   │   │   │   ├── decode.c                                 # File: decode
│   │   │   │   │   ├── huffman.c                                # File: huffman
│   │   │   │   │   ├── huffman.h                                # File: huffman
│   │   │   │   │   ├── prefix.h                                 # File: prefix
│   │   │   │   │   ├── state.c                                  # File: state
│   │   │   │   │   └── state.h                                  # File: state
│   │   │   │   ├── enc/                                         # Enc
│   │   │   │   │   ├── backward_references.c                    # File: backward references
│   │   │   │   │   ├── backward_references.h                    # File: backward references
│   │   │   │   │   ├── backward_references_hq.c                 # File: backward references hq
│   │   │   │   │   ├── backward_references_hq.h                 # File: backward references hq
│   │   │   │   │   ├── backward_references_inc.h                # File: backward references inc
│   │   │   │   │   ├── bit_cost.c                               # File: bit cost
│   │   │   │   │   ├── bit_cost.h                               # File: bit cost
│   │   │   │   │   ├── bit_cost_inc.h                           # File: bit cost inc
│   │   │   │   │   ├── block_encoder_inc.h                      # File: block encoder inc
│   │   │   │   │   ├── block_splitter.c                         # File: block splitter
│   │   │   │   │   ├── block_splitter.h                         # File: block splitter
│   │   │   │   │   ├── block_splitter_inc.h                     # File: block splitter inc
│   │   │   │   │   ├── brotli_bit_stream.c                      # File: brotli bit stream
│   │   │   │   │   ├── brotli_bit_stream.h                      # File: brotli bit stream
│   │   │   │   │   ├── cluster.c                                # File: cluster
│   │   │   │   │   ├── cluster.h                                # File: cluster
│   │   │   │   │   ├── cluster_inc.h                            # File: cluster inc
│   │   │   │   │   ├── command.c                                # File: command
│   │   │   │   │   ├── command.h                                # File: command
│   │   │   │   │   ├── compound_dictionary.c                    # File: compound dictionary
│   │   │   │   │   ├── compound_dictionary.h                    # File: compound dictionary
│   │   │   │   │   ├── compress_fragment.c                      # File: compress fragment
│   │   │   │   │   ├── compress_fragment.h                      # File: compress fragment
│   │   │   │   │   ├── compress_fragment_two_pass.c             # File: compress fragment two pass
│   │   │   │   │   ├── compress_fragment_two_pass.h             # File: compress fragment two pass
│   │   │   │   │   ├── dictionary_hash.c                        # File: dictionary hash
│   │   │   │   │   ├── dictionary_hash.h                        # File: dictionary hash
│   │   │   │   │   ├── encode.c                                 # File: encode
│   │   │   │   │   ├── encoder_dict.c                           # File: encoder dict
│   │   │   │   │   ├── encoder_dict.h                           # File: encoder dict
│   │   │   │   │   ├── entropy_encode.c                         # File: entropy encode
│   │   │   │   │   ├── entropy_encode.h                         # File: entropy encode
│   │   │   │   │   ├── entropy_encode_static.h                  # File: entropy encode static
│   │   │   │   │   ├── fast_log.c                               # File: fast log
│   │   │   │   │   ├── fast_log.h                               # File: fast log
│   │   │   │   │   ├── find_match_length.h                      # File: find match length
│   │   │   │   │   ├── hash.h                                   # File: hash
│   │   │   │   │   ├── hash_composite_inc.h                     # File: hash composite inc
│   │   │   │   │   ├── hash_forgetful_chain_inc.h               # File: hash forgetful chain inc
│   │   │   │   │   ├── hash_longest_match64_inc.h               # File: hash longest match64 inc
│   │   │   │   │   ├── hash_longest_match_inc.h                 # File: hash longest match inc
│   │   │   │   │   ├── hash_longest_match_quickly_inc.h         # File: hash longest match quickly inc
│   │   │   │   │   ├── hash_rolling_inc.h                       # File: hash rolling inc
│   │   │   │   │   ├── hash_to_binary_tree_inc.h                # File: hash to binary tree inc
│   │   │   │   │   ├── histogram.c                              # File: histogram
│   │   │   │   │   ├── histogram.h                              # File: histogram
│   │   │   │   │   ├── histogram_inc.h                          # File: histogram inc
│   │   │   │   │   ├── literal_cost.c                           # File: literal cost
│   │   │   │   │   ├── literal_cost.h                           # File: literal cost
│   │   │   │   │   ├── memory.c                                 # File: memory
│   │   │   │   │   ├── memory.h                                 # File: memory
│   │   │   │   │   ├── metablock.c                              # File: metablock
│   │   │   │   │   ├── metablock.h                              # File: metablock
│   │   │   │   │   ├── metablock_inc.h                          # File: metablock inc
│   │   │   │   │   ├── params.h                                 # File: params
│   │   │   │   │   ├── prefix.h                                 # File: prefix
│   │   │   │   │   ├── quality.h                                # File: quality
│   │   │   │   │   ├── ringbuffer.h                             # File: ringbuffer
│   │   │   │   │   ├── state.h                                  # File: state
│   │   │   │   │   ├── static_dict.c                            # File: static dict
│   │   │   │   │   ├── static_dict.h                            # File: static dict
│   │   │   │   │   ├── static_dict_lut.h                        # File: static dict lut
│   │   │   │   │   ├── utf8_util.c                              # File: utf8 util
│   │   │   │   │   ├── utf8_util.h                              # File: utf8 util
│   │   │   │   │   └── write_bits.h                             # File: write bits
│   │   │   │   ├── fuzz/                                        # Fuzz
│   │   │   │   │   ├── BUILD.bazel                              # File: BUILD
│   │   │   │   │   ├── WORKSPACE.bazel                          # File: WORKSPACE
│   │   │   │   │   ├── decode_fuzzer.c                          # File: decode fuzzer
│   │   │   │   │   ├── run_decode_fuzzer.c                      # File: run decode fuzzer
│   │   │   │   │   └── test_fuzzer.sh                           # Shell script: test fuzzer
│   │   │   │   ├── include/                                     # Include
│   │   │   │   │   └── brotli/                                  # Brotli
│   │   │   │   │       ├── decode.h                             # File: decode
│   │   │   │   │       ├── encode.h                             # File: encode
│   │   │   │   │       ├── port.h                               # File: port
│   │   │   │   │       ├── shared_dictionary.h                  # File: shared dictionary
│   │   │   │   │       └── types.h                              # File: types
│   │   │   │   └── tools/                                       # Tools
│   │   │   │       ├── brotli.c                                 # File: brotli
│   │   │   │       └── brotli.md                                # NAME
│   │   │   ├── docs/                                            # Documentation
│   │   │   │   ├── brotli-comparison-study-2015-09-22.pdf       # File: brotli comparison study 2015 09 22
│   │   │   │   ├── brotli.1                                     # File: brotli
│   │   │   │   ├── brotli.svg                                   # SVG image: brotli
│   │   │   │   ├── constants.h.3                                # File: constants.h
│   │   │   │   ├── decode.h.3                                   # File: decode.h
│   │   │   │   ├── encode.h.3                                   # File: encode.h
│   │   │   │   └── types.h.3                                    # File: types.h
│   │   │   ├── fetch-spec/                                      # Fetch spec
│   │   │   │   └── shared-brotli-fetch-spec.txt                 # Text: shared brotli fetch spec
│   │   │   ├── scripts/                                         # Maintenance scripts
│   │   │   │   ├── dictionary/                                  # Set of tools that can be used to download brotli RFC, extract and validate binary dictionary, and generate dictionary derivatives (e.g
│   │   │   │   │   ├── README.md                                # Set of tools that can be used to download brotli RFC, extract and validate binary dictionary, and generate dictionary derivatives (e.g
│   │   │   │   │   ├── step-01-download-rfc.py                  # Step 01 - download RFC7932
│   │   │   │   │   ├── step-02-rfc-to-bin.py                    # Step 02 - parse RFC
│   │   │   │   │   ├── step-03-validate-bin.py                  # Step 03 - validate raw dictionary file
│   │   │   │   │   └── step-04-generate-java-literals.py        # Step 04 - generate Java literals
│   │   │   │   ├── download_testdata.sh                         # Shell script: download testdata
│   │   │   │   ├── libbrotlicommon.pc.in                        # File: libbrotlicommon.pc
│   │   │   │   ├── libbrotlidec.pc.in                           # File: libbrotlidec.pc
│   │   │   │   └── libbrotlienc.pc.in                           # File: libbrotlienc.pc
│   │   │   ├── CHANGELOG.md                                     # All notable changes to this project will be documented in this file
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── CONTRIBUTING.md                                  # Want to contribute?
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── README                                           # File: README
│   │   │   ├── README.md                                        # Brotli is a generic-purpose lossless compression algorithm that compresses data using a combination of a modern variant of the LZ77
│   │   │   └── SECURITY.md                                      # To report a security issue, please use https://g.co/vulnz
│   │   ├── cppjieba/                                            # Cppjieba
│   │   │   ├── deps/                                            # Deps
│   │   │   │   └── limonp/                                      # Limonp
│   │   │   │       ├── include/                                 # Include
│   │   │   │       │   └── limonp/                              # Limonp
│   │   │   │       │       ├── ArgvContext.hpp                  # File: ArgvContext
│   │   │   │       │       ├── Closure.hpp                      # File: Closure
│   │   │   │       │       ├── Colors.hpp                       # File: Colors
│   │   │   │       │       ├── Condition.hpp                    # File: Condition
│   │   │   │       │       ├── Config.hpp                       # File: Config
│   │   │   │       │       ├── ForcePublic.hpp                  # File: ForcePublic
│   │   │   │       │       ├── LocalVector.hpp                  # File: LocalVector
│   │   │   │       │       ├── Logging.hpp                      # File: Logging
│   │   │   │       │       ├── NonCopyable.hpp                  # File: NonCopyable
│   │   │   │       │       ├── StdExtension.hpp                 # File: StdExtension
│   │   │   │       │       └── StringUtil.hpp                   # File: StringUtil
│   │   │   │       ├── .gitignore                               # Paths git never tracks
│   │   │   │       ├── .gitmodules                              # File: gitmodules
│   │   │   │       ├── CHANGELOG.md                             # + [CI] Update GitHub Actions configurations - Add stale issues workflow - Update checkout action from v2 to v4 - Update macOS test
│   │   │   │       ├── CMakeLists.txt                           # Text: CMakeLists
│   │   │   │       └── LICENSE                                  # The licence this repository is distributed under
│   │   │   ├── dict/                                            # 文件后缀名代表的是词典的编码方式。 比如filename.utf8 是 utf8编码，filename.gbk 是 gbk编码方式。
│   │   │   │   ├── pos_dict/                                    # Pos dict
│   │   │   │   │   ├── char_state_tab.utf8                      # File: char state tab
│   │   │   │   │   ├── prob_emit.utf8                           # File: prob emit
│   │   │   │   │   ├── prob_start.utf8                          # File: prob start
│   │   │   │   │   └── prob_trans.utf8                          # File: prob trans
│   │   │   │   ├── README.md                                    # 文件后缀名代表的是词典的编码方式。 比如filename.utf8 是 utf8编码，filename.gbk 是 gbk编码方式。
│   │   │   │   ├── hmm_model.utf8                               # File: hmm model
│   │   │   │   ├── idf.utf8                                     # File: idf
│   │   │   │   ├── jieba.dict.utf8                              # File: jieba.dict
│   │   │   │   ├── stop_words.utf8                              # File: stop words
│   │   │   │   └── user.dict.utf8                               # File: user.dict
│   │   │   ├── include/                                         # Include
│   │   │   │   └── cppjieba/                                    # Cppjieba
│   │   │   │       ├── DictTrie.hpp                             # File: DictTrie
│   │   │   │       ├── FullSegment.hpp                          # File: FullSegment
│   │   │   │       ├── HMMModel.hpp                             # File: HMMModel
│   │   │   │       ├── HMMSegment.hpp                           # File: HMMSegment
│   │   │   │       ├── Jieba.hpp                                # File: Jieba
│   │   │   │       ├── KeywordExtractor.hpp                     # File: KeywordExtractor
│   │   │   │       ├── MPSegment.hpp                            # File: MPSegment
│   │   │   │       ├── MixSegment.hpp                           # File: MixSegment
│   │   │   │       ├── PosTagger.hpp                            # File: PosTagger
│   │   │   │       ├── PreFilter.hpp                            # File: PreFilter
│   │   │   │       ├── QuerySegment.hpp                         # File: QuerySegment
│   │   │   │       ├── SegmentBase.hpp                          # File: SegmentBase
│   │   │   │       ├── SegmentTagged.hpp                        # File: SegmentTagged
│   │   │   │       ├── TextRankExtractor.hpp                    # File: TextRankExtractor
│   │   │   │       ├── Trie.hpp                                 # File: Trie
│   │   │   │       └── Unicode.hpp                              # File: Unicode
│   │   │   ├── .gitignore                                       # Paths git never tracks
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   └── VERSION                                          # File: VERSION
│   │   ├── fast_float/                                          # Fast float
│   │   │   ├── include/                                         # Include
│   │   │   │   └── fast_float.h                                 # File: fast float
│   │   │   └── CMakeLists.txt                                   # Text: CMakeLists
│   │   ├── fastpfor/                                            # Fastpfor
│   │   │   ├── fastpfor/                                        # Fastpfor
│   │   │   │   ├── bitpacking.cpp                               # File: bitpacking
│   │   │   │   ├── bitpacking.h                                 # File: bitpacking
│   │   │   │   ├── bitpackinghelpers.h                          # File: bitpackinghelpers
│   │   │   │   └── common.h                                     # File: common
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   └── README                                           # File: README
│   │   ├── glob/                                                # Glob
│   │   │   ├── glob/                                            # Glob
│   │   │   │   └── glob.hpp                                     # File: glob
│   │   │   └── CMakeLists.txt                                   # Text: CMakeLists
│   │   ├── httplib/                                             # Httplib
│   │   │   └── httplib.h                                        # File: httplib
│   │   ├── lz4/                                                 # Lz4
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── lz4.cpp                                          # File: lz4
│   │   │   └── lz4.hpp                                          # File: lz4
│   │   ├── mbedtls/                                             # Mbedtls
│   │   │   ├── include/                                         # Include
│   │   │   │   └── mbedtls/                                     # Mbedtls
│   │   │   │       ├── aes.h                                    # File: aes
│   │   │   │       ├── aria.h                                   # File: aria
│   │   │   │       ├── asn1.h                                   # File: asn1
│   │   │   │       ├── base64.h                                 # File: base64
│   │   │   │       ├── bignum.h                                 # File: bignum
│   │   │   │       ├── build_info.h                             # File: build info
│   │   │   │       ├── camellia.h                               # File: camellia
│   │   │   │       ├── ccm.h                                    # File: ccm
│   │   │   │       ├── check_config.h                           # File: check config
│   │   │   │       ├── cipher.h                                 # File: cipher
│   │   │   │       ├── constant_time.h                          # File: constant time
│   │   │   │       ├── des.h                                    # File: des
│   │   │   │       ├── entropy.h                                # File: entropy
│   │   │   │       ├── error.h                                  # File: error
│   │   │   │       ├── gcm.h                                    # File: gcm
│   │   │   │       ├── mbedtls_config.h                         # File: mbedtls config
│   │   │   │       ├── md.h                                     # File: md
│   │   │   │       ├── memory_buffer_alloc.h                    # File: memory buffer alloc
│   │   │   │       ├── oid.h                                    # File: oid
│   │   │   │       ├── pem.h                                    # File: pem
│   │   │   │       ├── pk.h                                     # File: pk
│   │   │   │       ├── platform.h                               # File: platform
│   │   │   │       ├── platform_time.h                          # File: platform time
│   │   │   │       ├── platform_util.h                          # File: platform util
│   │   │   │       ├── private_access.h                         # File: private access
│   │   │   │       ├── rsa.h                                    # File: rsa
│   │   │   │       ├── sha1.h                                   # File: sha1
│   │   │   │       ├── sha256.h                                 # File: sha256
│   │   │   │       └── sha512.h                                 # File: sha512
│   │   │   ├── library/                                         # Library
│   │   │   │   ├── aes.cpp                                      # File: aes
│   │   │   │   ├── aria.cpp                                     # File: aria
│   │   │   │   ├── asn1parse.cpp                                # File: asn1parse
│   │   │   │   ├── base64.cpp                                   # File: base64
│   │   │   │   ├── bignum.cpp                                   # File: bignum
│   │   │   │   ├── bn_mul.h                                     # File: bn mul
│   │   │   │   ├── camellia.cpp                                 # File: camellia
│   │   │   │   ├── cipher.cpp                                   # File: cipher
│   │   │   │   ├── cipher_wrap.cpp                              # File: cipher wrap
│   │   │   │   ├── cipher_wrap.h                                # File: cipher wrap
│   │   │   │   ├── common.h                                     # File: common
│   │   │   │   ├── constant_time.cpp                            # File: constant time
│   │   │   │   ├── constant_time_internal.h                     # File: constant time internal
│   │   │   │   ├── constant_time_invasive.h                     # File: constant time invasive
│   │   │   │   ├── entropy.cpp                                  # File: entropy
│   │   │   │   ├── entropy_poll.cpp                             # File: entropy poll
│   │   │   │   ├── entropy_poll.h                               # File: entropy poll
│   │   │   │   ├── gcm.cpp                                      # File: gcm
│   │   │   │   ├── md.cpp                                       # File: md
│   │   │   │   ├── md_wrap.h                                    # File: md wrap
│   │   │   │   ├── oid.cpp                                      # File: oid
│   │   │   │   ├── pem.cpp                                      # File: pem
│   │   │   │   ├── pk.cpp                                       # File: pk
│   │   │   │   ├── pk_wrap.cpp                                  # File: pk wrap
│   │   │   │   ├── pk_wrap.h                                    # File: pk wrap
│   │   │   │   ├── pkparse.cpp                                  # File: pkparse
│   │   │   │   ├── platform_util.cpp                            # File: platform util
│   │   │   │   ├── rsa.cpp                                      # File: rsa
│   │   │   │   ├── rsa_alt_helpers.cpp                          # File: rsa alt helpers
│   │   │   │   ├── rsa_alt_helpers.h                            # File: rsa alt helpers
│   │   │   │   ├── sha1.cpp                                     # File: sha1
│   │   │   │   ├── sha256.cpp                                   # File: sha256
│   │   │   │   └── sha512.cpp                                   # File: sha512
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── miniz/                                               # Miniz
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── miniz.cpp                                        # File: miniz
│   │   │   ├── miniz.hpp                                        # File: miniz
│   │   │   └── miniz_wrapper.hpp                                # File: miniz wrapper
│   │   ├── parquet/                                             # Parquet
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── parquet_constants.cpp                            # File: parquet constants
│   │   │   ├── parquet_constants.h                              # File: parquet constants
│   │   │   ├── parquet_types.cpp                                # File: parquet types
│   │   │   ├── parquet_types.h                                  # File: parquet types
│   │   │   └── windows_compatibility.h                          # File: windows compatibility
│   │   ├── pcg/                                                 # Pcg
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── pcg_extras.hpp                                   # File: pcg extras
│   │   │   ├── pcg_random.hpp                                   # File: pcg random
│   │   │   └── pcg_uint128.hpp                                  # File: pcg uint128
│   │   ├── ports/                                               # Ports
│   │   │   ├── lz4/                                             # Lz4
│   │   │   │   ├── Makefile                                     # LZ4 port Fast LZ compression library
│   │   │   │   └── amalgamate.sh                                # Amalgamate lz4 source files for third_party
│   │   │   ├── mbedtls/                                         # Mbedtls
│   │   │   │   ├── CMakeLists.txt.custom                        # File: CMakeLists.txt
│   │   │   │   ├── Makefile                                     # mbedtls port C crypto library - source files are copied directly, no build needed
│   │   │   │   └── do-patch.py                                  # Patch script for mbedtls
│   │   │   ├── roaring_bitmap/                                  # Roaring bitmap
│   │   │   │   ├── CMakeLists.txt.custom                        # File: CMakeLists.txt
│   │   │   │   └── Makefile                                     # RoaringBitmap port CRoaring library - Fast bitmap data structure
│   │   │   ├── spdlog/                                          # Spdlog
│   │   │   │   └── Makefile                                     # spdlog port Fast C++ logging library
│   │   │   ├── zstd/                                            # When updating zstd to 1.5.7, the new version includes x86-64 assembly optimizations (huf_decompress_amd64.S) that can cause linker errors
│   │   │   │   ├── Makefile                                     # Zstd port Zstandard - Fast lossless compression algorithm
│   │   │   │   ├── README.md                                    # When updating zstd to 1.5.7, the new version includes x86-64 assembly optimizations (huf_decompress_amd64.S) that can cause linker errors
│   │   │   │   └── do-patch.py                                  # Patch script for zstd Applies necessary transformations to the zstd source code for integration into the ladybug codebase
│   │   │   └── bsd.port.mk                                      # File: bsd.port
│   │   ├── pybind11/                                            # Pybind11
│   │   │   ├── include/                                         # Include
│   │   │   │   └── pybind11/                                    # Pybind11
│   │   │   │       ├── detail/                                  # Detail
│   │   │   │       │   ├── class.h                              # File: class
│   │   │   │       │   ├── common.h                             # File: common
│   │   │   │       │   ├── descr.h                              # File: descr
│   │   │   │       │   ├── init.h                               # File: init
│   │   │   │       │   ├── internals.h                          # File: internals
│   │   │   │       │   ├── type_caster_base.h                   # File: type caster base
│   │   │   │       │   └── typeid.h                             # File: typeid
│   │   │   │       ├── eigen/                                   # Eigen
│   │   │   │       │   ├── common.h                             # File: common
│   │   │   │       │   ├── matrix.h                             # File: matrix
│   │   │   │       │   └── tensor.h                             # File: tensor
│   │   │   │       ├── stl/                                     # Stl
│   │   │   │       │   └── filesystem.h                         # File: filesystem
│   │   │   │       ├── attr.h                                   # File: attr
│   │   │   │       ├── buffer_info.h                            # File: buffer info
│   │   │   │       ├── cast.h                                   # File: cast
│   │   │   │       ├── chrono.h                                 # File: chrono
│   │   │   │       ├── common.h                                 # File: common
│   │   │   │       ├── complex.h                                # File: complex
│   │   │   │       ├── eigen.h                                  # File: eigen
│   │   │   │       ├── embed.h                                  # File: embed
│   │   │   │       ├── eval.h                                   # File: eval
│   │   │   │       ├── functional.h                             # File: functional
│   │   │   │       ├── gil.h                                    # File: gil
│   │   │   │       ├── gil_safe_call_once.h                     # File: gil safe call once
│   │   │   │       ├── iostream.h                               # File: iostream
│   │   │   │       ├── numpy.h                                  # File: numpy
│   │   │   │       ├── operators.h                              # File: operators
│   │   │   │       ├── options.h                                # File: options
│   │   │   │       ├── pybind11.h                               # File: pybind11
│   │   │   │       ├── pytypes.h                                # File: pytypes
│   │   │   │       ├── stl.h                                    # File: stl
│   │   │   │       ├── stl_bind.h                               # File: stl bind
│   │   │   │       ├── type_caster_pyobject_ptr.h               # File: type caster pyobject ptr
│   │   │   │       └── typing.h                                 # File: typing
│   │   │   ├── tools/                                           # Tools
│   │   │   │   ├── FindCatch.cmake                              # File: FindCatch
│   │   │   │   ├── FindEigen3.cmake                             # File: FindEigen3
│   │   │   │   ├── FindPythonLibsNew.cmake                      # File: FindPythonLibsNew
│   │   │   │   ├── JoinPaths.cmake                              # File: JoinPaths
│   │   │   │   ├── check-style.sh                               # Script to check include/test code for common pybind11 code style errors
│   │   │   │   ├── cmake_uninstall.cmake.in                     # File: cmake uninstall.cmake
│   │   │   │   ├── codespell_ignore_lines_from_errors.py        # Simple script for rebuilding .codespell-ignore-lines
│   │   │   │   ├── libsize.py                                   # Internal build script for generating debugging test .so size
│   │   │   │   ├── make_changelog.py                            # Python script: make changelog
│   │   │   │   ├── pybind11.pc.in                               # File: pybind11.pc
│   │   │   │   ├── pybind11Common.cmake                         # File: pybind11Common
│   │   │   │   ├── pybind11Config.cmake.in                      # File: pybind11Config.cmake
│   │   │   │   ├── pybind11GuessPythonExtSuffix.cmake           # File: pybind11GuessPythonExtSuffix
│   │   │   │   ├── pybind11NewTools.cmake                       # File: pybind11NewTools
│   │   │   │   ├── pybind11Tools.cmake                          # File: pybind11Tools
│   │   │   │   ├── pyproject.toml                               # TOML settings: pyproject
│   │   │   │   ├── setup_global.py.in                           # File: setup global.py
│   │   │   │   ├── setup_main.py.in                             # File: setup main.py
│   │   │   │   └── test-pybind11GuessPythonExtSuffix.cmake      # File: test pybind11GuessPythonExtSuffix
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── pyparse/                                             # Pyparse
│   │   │   └── pyparse.h                                        # File: pyparse
│   │   ├── re2/                                                 # Re2
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── bitmap256.h                                  # File: bitmap256
│   │   │   │   ├── filtered_re2.h                               # File: filtered re2
│   │   │   │   ├── logging.h                                    # File: logging
│   │   │   │   ├── mix.h                                        # File: mix
│   │   │   │   ├── mutex.h                                      # File: mutex
│   │   │   │   ├── pod_array.h                                  # File: pod array
│   │   │   │   ├── prefilter.h                                  # File: prefilter
│   │   │   │   ├── prefilter_tree.h                             # File: prefilter tree
│   │   │   │   ├── prog.h                                       # File: prog
│   │   │   │   ├── re2.h                                        # File: re2
│   │   │   │   ├── regexp.h                                     # File: regexp
│   │   │   │   ├── set.h                                        # File: set
│   │   │   │   ├── sparse_array.h                               # File: sparse array
│   │   │   │   ├── sparse_set.h                                 # File: sparse set
│   │   │   │   ├── stringpiece.h                                # File: stringpiece
│   │   │   │   ├── strutil.h                                    # File: strutil
│   │   │   │   ├── unicode_casefold.h                           # File: unicode casefold
│   │   │   │   ├── unicode_groups.h                             # File: unicode groups
│   │   │   │   ├── utf.h                                        # File: utf
│   │   │   │   ├── util.h                                       # File: util
│   │   │   │   └── walker-inl.h                                 # File: walker inl
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── bitstate.cpp                                     # File: bitstate
│   │   │   ├── compile.cpp                                      # File: compile
│   │   │   ├── dfa.cpp                                          # File: dfa
│   │   │   ├── filtered_re2.cpp                                 # File: filtered re2
│   │   │   ├── mimics_pcre.cpp                                  # File: mimics pcre
│   │   │   ├── nfa.cpp                                          # File: nfa
│   │   │   ├── onepass.cpp                                      # File: onepass
│   │   │   ├── parse.cpp                                        # File: parse
│   │   │   ├── perl_groups.cpp                                  # File: perl groups
│   │   │   ├── prefilter.cpp                                    # File: prefilter
│   │   │   ├── prefilter_tree.cpp                               # File: prefilter tree
│   │   │   ├── prog.cpp                                         # File: prog
│   │   │   ├── re2.cpp                                          # File: re2
│   │   │   ├── regexp.cpp                                       # File: regexp
│   │   │   ├── rune.cpp                                         # File: rune
│   │   │   ├── set.cpp                                          # File: set
│   │   │   ├── simplify.cpp                                     # File: simplify
│   │   │   ├── stringpiece.cpp                                  # File: stringpiece
│   │   │   ├── strutil.cpp                                      # File: strutil
│   │   │   ├── tostring.cpp                                     # File: tostring
│   │   │   ├── unicode_casefold.cpp                             # File: unicode casefold
│   │   │   └── unicode_groups.cpp                               # File: unicode groups
│   │   ├── roaring_bitmap/                                      # Roaring bitmap
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── roaring.c                                        # File: roaring
│   │   │   ├── roaring.h                                        # File: roaring
│   │   │   └── roaring.hh                                       # File: roaring
│   │   ├── simsimd/                                             # Simsimd
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── binary.h                                     # File: binary
│   │   │   │   ├── curved.h                                     # File: curved
│   │   │   │   ├── dot.h                                        # File: dot
│   │   │   │   ├── elementwise.h                                # File: elementwise
│   │   │   │   ├── geospatial.h                                 # File: geospatial
│   │   │   │   ├── mesh.h                                       # File: mesh
│   │   │   │   ├── probability.h                                # File: probability
│   │   │   │   ├── simsimd.h                                    # File: simsimd
│   │   │   │   ├── sparse.h                                     # File: sparse
│   │   │   │   ├── spatial.h                                    # File: spatial
│   │   │   │   └── types.h                                      # File: types
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   └── lib.c                                            # File: lib
│   │   ├── snappy/                                              # Snappy
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── snappy-internal.h                                # File: snappy internal
│   │   │   ├── snappy-sinksource.cc                             # File: snappy sinksource
│   │   │   ├── snappy-sinksource.h                              # File: snappy sinksource
│   │   │   ├── snappy-stubs-internal.cc                         # File: snappy stubs internal
│   │   │   ├── snappy-stubs-internal.h                          # File: snappy stubs internal
│   │   │   ├── snappy-stubs-public.h                            # File: snappy stubs public
│   │   │   ├── snappy-stubs-public.h.in                         # File: snappy stubs public.h
│   │   │   ├── snappy.cc                                        # File: snappy
│   │   │   ├── snappy.h                                         # File: snappy
│   │   │   └── snappy_version.hpp                               # File: snappy version
│   │   ├── spdlog/                                              # Spdlog
│   │   │   ├── spdlog/                                          # Spdlog
│   │   │   │   ├── cfg/                                         # Cfg
│   │   │   │   │   ├── argv.h                                   # File: argv
│   │   │   │   │   ├── env.h                                    # File: env
│   │   │   │   │   ├── helpers-inl.h                            # File: helpers inl
│   │   │   │   │   └── helpers.h                                # File: helpers
│   │   │   │   ├── details/                                     # Details
│   │   │   │   │   ├── backtracer-inl.h                         # File: backtracer inl
│   │   │   │   │   ├── backtracer.h                             # File: backtracer
│   │   │   │   │   ├── circular_q.h                             # File: circular q
│   │   │   │   │   ├── console_globals.h                        # File: console globals
│   │   │   │   │   ├── file_helper-inl.h                        # File: file helper inl
│   │   │   │   │   ├── file_helper.h                            # File: file helper
│   │   │   │   │   ├── fmt_helper.h                             # File: fmt helper
│   │   │   │   │   ├── log_msg-inl.h                            # File: log msg inl
│   │   │   │   │   ├── log_msg.h                                # File: log msg
│   │   │   │   │   ├── log_msg_buffer-inl.h                     # File: log msg buffer inl
│   │   │   │   │   ├── log_msg_buffer.h                         # File: log msg buffer
│   │   │   │   │   ├── mpmc_blocking_q.h                        # File: mpmc blocking q
│   │   │   │   │   ├── null_mutex.h                             # File: null mutex
│   │   │   │   │   ├── os-inl.h                                 # File: os inl
│   │   │   │   │   ├── os.h                                     # File: os
│   │   │   │   │   ├── periodic_worker-inl.h                    # File: periodic worker inl
│   │   │   │   │   ├── periodic_worker.h                        # File: periodic worker
│   │   │   │   │   ├── registry-inl.h                           # File: registry inl
│   │   │   │   │   ├── registry.h                               # File: registry
│   │   │   │   │   ├── synchronous_factory.h                    # File: synchronous factory
│   │   │   │   │   ├── tcp_client-windows.h                     # File: tcp client windows
│   │   │   │   │   ├── tcp_client.h                             # File: tcp client
│   │   │   │   │   ├── thread_pool-inl.h                        # File: thread pool inl
│   │   │   │   │   ├── thread_pool.h                            # File: thread pool
│   │   │   │   │   ├── udp_client-windows.h                     # File: udp client windows
│   │   │   │   │   ├── udp_client.h                             # File: udp client
│   │   │   │   │   └── windows_include.h                        # File: windows include
│   │   │   │   ├── fmt/                                         # Fmt
│   │   │   │   │   ├── bundled/                                 # Bundled
│   │   │   │   │   │   ├── args.h                               # File: args
│   │   │   │   │   │   ├── chrono.h                             # File: chrono
│   │   │   │   │   │   ├── color.h                              # File: color
│   │   │   │   │   │   ├── compile.h                            # File: compile
│   │   │   │   │   │   ├── core.h                               # File: core
│   │   │   │   │   │   ├── fmt.license.rst                      # File: fmt.license
│   │   │   │   │   │   ├── format-inl.h                         # File: format inl
│   │   │   │   │   │   ├── format.h                             # File: format
│   │   │   │   │   │   ├── locale.h                             # File: locale
│   │   │   │   │   │   ├── os.h                                 # File: os
│   │   │   │   │   │   ├── ostream.h                            # File: ostream
│   │   │   │   │   │   ├── printf.h                             # File: printf
│   │   │   │   │   │   ├── ranges.h                             # File: ranges
│   │   │   │   │   │   ├── std.h                                # File: std
│   │   │   │   │   │   └── xchar.h                              # File: xchar
│   │   │   │   │   ├── bin_to_hex.h                             # File: bin to hex
│   │   │   │   │   ├── chrono.h                                 # File: chrono
│   │   │   │   │   ├── compile.h                                # File: compile
│   │   │   │   │   ├── fmt.h                                    # File: fmt
│   │   │   │   │   ├── ostr.h                                   # File: ostr
│   │   │   │   │   ├── ranges.h                                 # File: ranges
│   │   │   │   │   ├── std.h                                    # File: std
│   │   │   │   │   └── xchar.h                                  # File: xchar
│   │   │   │   ├── sinks/                                       # Sinks
│   │   │   │   │   ├── android_sink.h                           # File: android sink
│   │   │   │   │   ├── ansicolor_sink-inl.h                     # File: ansicolor sink inl
│   │   │   │   │   ├── ansicolor_sink.h                         # File: ansicolor sink
│   │   │   │   │   ├── base_sink-inl.h                          # File: base sink inl
│   │   │   │   │   ├── base_sink.h                              # File: base sink
│   │   │   │   │   ├── basic_file_sink-inl.h                    # File: basic file sink inl
│   │   │   │   │   ├── basic_file_sink.h                        # File: basic file sink
│   │   │   │   │   ├── callback_sink.h                          # File: callback sink
│   │   │   │   │   ├── daily_file_sink.h                        # File: daily file sink
│   │   │   │   │   ├── dist_sink.h                              # File: dist sink
│   │   │   │   │   ├── dup_filter_sink.h                        # File: dup filter sink
│   │   │   │   │   ├── hourly_file_sink.h                       # File: hourly file sink
│   │   │   │   │   ├── kafka_sink.h                             # File: kafka sink
│   │   │   │   │   ├── mongo_sink.h                             # File: mongo sink
│   │   │   │   │   ├── msvc_sink.h                              # File: msvc sink
│   │   │   │   │   ├── null_sink.h                              # File: null sink
│   │   │   │   │   ├── ostream_sink.h                           # File: ostream sink
│   │   │   │   │   ├── qt_sinks.h                               # File: qt sinks
│   │   │   │   │   ├── ringbuffer_sink.h                        # File: ringbuffer sink
│   │   │   │   │   ├── rotating_file_sink-inl.h                 # File: rotating file sink inl
│   │   │   │   │   ├── rotating_file_sink.h                     # File: rotating file sink
│   │   │   │   │   ├── sink-inl.h                               # File: sink inl
│   │   │   │   │   ├── sink.h                                   # File: sink
│   │   │   │   │   ├── stdout_color_sinks-inl.h                 # File: stdout color sinks inl
│   │   │   │   │   ├── stdout_color_sinks.h                     # File: stdout color sinks
│   │   │   │   │   ├── stdout_sinks-inl.h                       # File: stdout sinks inl
│   │   │   │   │   ├── stdout_sinks.h                           # File: stdout sinks
│   │   │   │   │   ├── syslog_sink.h                            # File: syslog sink
│   │   │   │   │   ├── systemd_sink.h                           # File: systemd sink
│   │   │   │   │   ├── tcp_sink.h                               # File: tcp sink
│   │   │   │   │   ├── udp_sink.h                               # File: udp sink
│   │   │   │   │   ├── win_eventlog_sink.h                      # File: win eventlog sink
│   │   │   │   │   ├── wincolor_sink-inl.h                      # File: wincolor sink inl
│   │   │   │   │   └── wincolor_sink.h                          # File: wincolor sink
│   │   │   │   ├── async.h                                      # File: async
│   │   │   │   ├── async_logger-inl.h                           # File: async logger inl
│   │   │   │   ├── async_logger.h                               # File: async logger
│   │   │   │   ├── common-inl.h                                 # File: common inl
│   │   │   │   ├── common.h                                     # File: common
│   │   │   │   ├── formatter.h                                  # File: formatter
│   │   │   │   ├── fwd.h                                        # File: fwd
│   │   │   │   ├── logger-inl.h                                 # File: logger inl
│   │   │   │   ├── logger.h                                     # File: logger
│   │   │   │   ├── pattern_formatter-inl.h                      # File: pattern formatter inl
│   │   │   │   ├── pattern_formatter.h                          # File: pattern formatter
│   │   │   │   ├── spdlog-inl.h                                 # File: spdlog inl
│   │   │   │   ├── spdlog.h                                     # File: spdlog
│   │   │   │   ├── stopwatch.h                                  # File: stopwatch
│   │   │   │   ├── tweakme.h                                    # File: tweakme
│   │   │   │   └── version.h                                    # File: version
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── taywee_args/                                         # Taywee args
│   │   │   ├── include/                                         # Include
│   │   │   │   └── args.hxx                                     # File: args
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── thrift/                                              # Thrift
│   │   │   ├── protocol/                                        # Protocol
│   │   │   │   ├── TCompactProtocol.h                           # File: TCompactProtocol
│   │   │   │   ├── TCompactProtocol.tcc                         # File: TCompactProtocol
│   │   │   │   ├── TProtocol.cpp                                # File: TProtocol
│   │   │   │   ├── TProtocol.h                                  # File: TProtocol
│   │   │   │   ├── TProtocolDecorator.h                         # File: TProtocolDecorator
│   │   │   │   ├── TProtocolException.h                         # File: TProtocolException
│   │   │   │   ├── TProtocolTypes.h                             # File: TProtocolTypes
│   │   │   │   └── TVirtualProtocol.h                           # File: TVirtualProtocol
│   │   │   ├── transport/                                       # Transport
│   │   │   │   ├── PlatformSocket.h                             # File: PlatformSocket
│   │   │   │   ├── TBufferTransports.cpp                        # File: TBufferTransports
│   │   │   │   ├── TBufferTransports.h                          # File: TBufferTransports
│   │   │   │   ├── TTransport.h                                 # File: TTransport
│   │   │   │   ├── TTransportException.cpp                      # File: TTransportException
│   │   │   │   ├── TTransportException.h                        # File: TTransportException
│   │   │   │   └── TVirtualTransport.h                          # File: TVirtualTransport
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   ├── TApplicationException.h                          # File: TApplicationException
│   │   │   ├── TBase.h                                          # File: TBase
│   │   │   ├── TLogging.h                                       # File: TLogging
│   │   │   ├── TToString.h                                      # File: TToString
│   │   │   ├── Thrift.h                                         # File: Thrift
│   │   │   ├── stdcxx.h                                         # File: stdcxx
│   │   │   ├── thrift-config.h                                  # File: thrift config
│   │   │   └── thrift_export.h                                  # File: thrift export
│   │   ├── utf8proc/                                            # Utf8proc
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── utf8proc.h                                   # File: utf8proc
│   │   │   │   ├── utf8proc_data.h                              # File: utf8proc data
│   │   │   │   └── utf8proc_wrapper.h                           # File: utf8proc wrapper
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE.md                                       # utf8proc is a software package originally developed by Jan Behrens and the rest of the Public Software Group
│   │   │   ├── utf8proc.cpp                                     # File: utf8proc
│   │   │   └── utf8proc_wrapper.cpp                             # File: utf8proc wrapper
│   │   ├── yyjson/                                              # A high performance JSON library written in ANSI C
│   │   │   ├── src/                                             # The crate's sources
│   │   │   │   ├── yyjson.c                                     # File: yyjson
│   │   │   │   └── yyjson.h                                     # File: yyjson
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   ├── LICENSE                                          # The licence this repository is distributed under
│   │   │   └── README.md                                        # A high performance JSON library written in ANSI C
│   │   ├── zstd/                                                # Zstd
│   │   │   ├── common/                                          # Common
│   │   │   │   ├── entropy_common.cpp                           # File: entropy common
│   │   │   │   ├── error_private.cpp                            # File: error private
│   │   │   │   ├── fse_decompress.cpp                           # File: fse decompress
│   │   │   │   ├── xxhash.cpp                                   # File: xxhash
│   │   │   │   └── zstd_common.cpp                              # File: zstd common
│   │   │   ├── compress/                                        # Compress
│   │   │   │   ├── fse_compress.cpp                             # File: fse compress
│   │   │   │   ├── hist.cpp                                     # File: hist
│   │   │   │   ├── huf_compress.cpp                             # File: huf compress
│   │   │   │   ├── zstd_compress.cpp                            # File: zstd compress
│   │   │   │   ├── zstd_compress_literals.cpp                   # File: zstd compress literals
│   │   │   │   ├── zstd_compress_sequences.cpp                  # File: zstd compress sequences
│   │   │   │   ├── zstd_compress_superblock.cpp                 # File: zstd compress superblock
│   │   │   │   ├── zstd_double_fast.cpp                         # File: zstd double fast
│   │   │   │   ├── zstd_fast.cpp                                # File: zstd fast
│   │   │   │   ├── zstd_lazy.cpp                                # File: zstd lazy
│   │   │   │   ├── zstd_ldm.cpp                                 # File: zstd ldm
│   │   │   │   ├── zstd_opt.cpp                                 # File: zstd opt
│   │   │   │   └── zstd_preSplit.cpp                            # File: zstd preSplit
│   │   │   ├── decompress/                                      # Decompress
│   │   │   │   ├── huf_decompress.cpp                           # File: huf decompress
│   │   │   │   ├── zstd_ddict.cpp                               # File: zstd ddict
│   │   │   │   ├── zstd_decompress.cpp                          # File: zstd decompress
│   │   │   │   └── zstd_decompress_block.cpp                    # File: zstd decompress block
│   │   │   ├── include/                                         # Include
│   │   │   │   ├── zstd/                                        # Zstd
│   │   │   │   │   ├── common/                                  # Common
│   │   │   │   │   │   ├── allocations.h                        # File: allocations
│   │   │   │   │   │   ├── bits.h                               # File: bits
│   │   │   │   │   │   ├── bitstream.h                          # File: bitstream
│   │   │   │   │   │   ├── compiler.h                           # File: compiler
│   │   │   │   │   │   ├── cpu.h                                # File: cpu
│   │   │   │   │   │   ├── debug.h                              # File: debug
│   │   │   │   │   │   ├── error_private.h                      # File: error private
│   │   │   │   │   │   ├── fse.h                                # File: fse
│   │   │   │   │   │   ├── fse_static.h                         # File: fse static
│   │   │   │   │   │   ├── huf.h                                # File: huf
│   │   │   │   │   │   ├── huf_static.h                         # File: huf static
│   │   │   │   │   │   ├── mem.h                                # File: mem
│   │   │   │   │   │   ├── pool.h                               # File: pool
│   │   │   │   │   │   ├── portability_macros.h                 # File: portability macros
│   │   │   │   │   │   ├── threading.h                          # File: threading
│   │   │   │   │   │   ├── xxhash.h                             # File: xxhash
│   │   │   │   │   │   ├── xxhash_static.h                      # File: xxhash static
│   │   │   │   │   │   ├── zstd_deps.h                          # File: zstd deps
│   │   │   │   │   │   ├── zstd_errors.h                        # File: zstd errors
│   │   │   │   │   │   ├── zstd_internal.h                      # File: zstd internal
│   │   │   │   │   │   └── zstd_trace.h                         # File: zstd trace
│   │   │   │   │   ├── compress/                                # Compress
│   │   │   │   │   │   ├── clevels.h                            # File: clevels
│   │   │   │   │   │   ├── hist.h                               # File: hist
│   │   │   │   │   │   ├── zstd_compress_internal.h             # File: zstd compress internal
│   │   │   │   │   │   ├── zstd_compress_literals.h             # File: zstd compress literals
│   │   │   │   │   │   ├── zstd_compress_sequences.h            # File: zstd compress sequences
│   │   │   │   │   │   ├── zstd_compress_superblock.h           # File: zstd compress superblock
│   │   │   │   │   │   ├── zstd_cwksp.h                         # File: zstd cwksp
│   │   │   │   │   │   ├── zstd_double_fast.h                   # File: zstd double fast
│   │   │   │   │   │   ├── zstd_fast.h                          # File: zstd fast
│   │   │   │   │   │   ├── zstd_lazy.h                          # File: zstd lazy
│   │   │   │   │   │   ├── zstd_ldm.h                           # File: zstd ldm
│   │   │   │   │   │   ├── zstd_ldm_geartab.h                   # File: zstd ldm geartab
│   │   │   │   │   │   ├── zstd_opt.h                           # File: zstd opt
│   │   │   │   │   │   ├── zstd_preSplit.h                      # File: zstd preSplit
│   │   │   │   │   │   └── zstdmt_compress.h                    # File: zstdmt compress
│   │   │   │   │   ├── decompress/                              # Decompress
│   │   │   │   │   │   ├── zstd_ddict.h                         # File: zstd ddict
│   │   │   │   │   │   ├── zstd_decompress_block.h              # File: zstd decompress block
│   │   │   │   │   │   └── zstd_decompress_internal.h           # File: zstd decompress internal
│   │   │   │   │   ├── zstd.h                                   # File: zstd
│   │   │   │   │   ├── zstd_errors.h                            # File: zstd errors
│   │   │   │   │   └── zstd_static.h                            # File: zstd static
│   │   │   │   ├── zstd.h                                       # File: zstd
│   │   │   │   ├── zstd_errors.h                                # File: zstd errors
│   │   │   │   └── zstd_static.h                                # File: zstd static
│   │   │   ├── CMakeLists.txt                                   # Text: CMakeLists
│   │   │   └── LICENSE                                          # The licence this repository is distributed under
│   │   ├── CMakeLists.txt                                       # Text: CMakeLists
│   │   ├── pkg                                                  # File: pkg
│   │   └── versions.txt                                         # Text: versions
│   ├── tools/                                                   # Tools
│   │   └── CMakeLists.txt                                       # Text: CMakeLists
│   ├── CMakeLists.txt                                           # Text: CMakeLists
│   └── README.md                                                # Ladybug is being developed by LadybugDB Developers and is available under a permissive license
├── scripts/                                                     # Maintenance scripts
│   ├── download-liblbug.sh                                      # Download prebuilt liblbug archives from GitHub releases or workflow artifacts
│   ├── download_lbug.ps1                                        # File: download lbug
│   └── download_lbug.sh                                         # Download prebuilt static liblbug into the Rust crate cache
├── src/                                                         # The crate's sources
│   ├── ffi/                                                     # Ffi
│   │   └── arrow.rs                                             # Rust source: arrow
│   ├── CMakeLists.txt                                           # Text: CMakeLists
│   ├── connection.rs                                            # Rust source: connection
│   ├── database.rs                                              # Rust source: database
│   ├── error.rs                                                 # Rust source: error
│   ├── ffi.rs                                                   # Rust source: ffi
│   ├── lbug_arrow.cpp                                           # File: lbug arrow
│   ├── lbug_rs.cpp                                              # File: lbug rs
│   ├── lib.rs                                                   # Bindings to Lbug: an in-process property graph database management system built for query speed and scalability
│   ├── logical_type.rs                                          # Rust source: logical type
│   ├── query_result.rs                                          # Rust source: query result
│   └── value.rs                                                 # Rust source: value
├── .cargo_vcs_info.json                                         # JSON data: cargo vcs info
├── Cargo.lock                                                   # Exact dependency versions, committed so every build resolves the same
├── Cargo.toml                                                   # Crate manifest: An in-process property graph database management system built for query speed and scalability
├── Cargo.toml.orig                                              # File: Cargo.toml
├── LICENSE                                                      # The licence this repository is distributed under
├── README.md                                                    # This repository carries lbug, the Rust binding of LadybugDB, with one small patch that Maestro needs
└── build.rs                                                     # Rust source: build
```

## Change and verification procedure

1. Read the rules in AGENTS.md that cover the files you change, and keep every
   gate intact: never weaken one to pass.
2. Add an executable regression check for a change in behaviour.
3. The commit hook `rust-gate guide` rewrites this guide when a file is added,
   moved or removed; commit it with the change. The organization's daily drift
   check reports a guide left stale.
4. Run `prek run --all-files`, and report the commands you actually ran.
5. Commits are signed, with a conventional title; the default branch takes only
   squash-merged pull requests.

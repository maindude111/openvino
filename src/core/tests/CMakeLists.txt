# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=gold")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
endif()

add_definitions(-DSERIALIZED_ZOO=\"${TEST_MODEL_ZOO}/core/models\")
set(OV_ONNX_NAMESPACE ngraph_onnx)

if(ENABLE_TESTS)
    add_definitions("-DTEST_FILES=\"${TEST_MODEL_ZOO}/core/files\"")
    add_subdirectory(util)
    add_subdirectory(engines_util)
    if (ENABLE_OV_ONNX_FRONTEND)
        add_subdirectory(onnx_test_util)
    endif()
endif()

if(NOT ENABLE_OV_CORE_UNIT_TESTS)
    message(STATUS "nGraph unit tests disabled")
    return()
endif()

message(STATUS "nGraph unit tests enabled")

if(LINUX)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
endif()

set(SRC
    aligned_buffer.cpp
    all_close_f.cpp
    bfloat16.cpp
    build_graph.cpp
    builder_autobroadcast.cpp
    check.cpp
    conditional_compilation/ngraph_cc_collect.cpp
    conditional_compilation/ngraph_cc_off.cpp
    conditional_compilation/ngraph_cc_on.cpp
    constant.cpp
    constant_folding.cpp
    control_dependencies.cpp
    convert_u1_to_string.cpp
    coordinate.cpp
    coordinate_range.cpp
    copy.cpp
    element_type.cpp
    eval.cpp
    extension.cpp
    file_util.cpp
    float16.cpp
    framework_node.cpp
    model.cpp
    graph_rewrite.cpp
    input_output_assign.cpp
    int4.cpp
    intervals.cpp
    layout.cpp
    main.cpp
    matcher_pass.cpp
    misc.cpp
    rtti.cpp
    node_input_output.cpp
    rtti.cpp
    op.cpp
    opset.cpp
    opset1.cpp
    ov_default_allocator_test.cpp
    ov_tensor_test.cpp
    any.cpp
    partial_shape.cpp
    pass_config.cpp
    pass_manager.cpp
    pass/serialization/cleanup.cpp
    pass/serialization/const_compression.cpp
    pass/serialization/deterministicity.cpp
    pass/serialization/serialize.cpp
    pattern.cpp
    preprocess.cpp
    replace_node.cpp
    reshape_opt_kernel.cpp
    shape.cpp
    span.cpp
    specialize_function.cpp
    tensor.cpp
    threading.cpp
    type_info.cpp
    type_prop/abs.cpp
    type_prop/acos.cpp
    type_prop/adaptive_avg_pool.cpp
    type_prop/adaptive_max_pool.cpp
    type_prop/add.cpp
    type_prop/asin.cpp
    type_prop/asinh.cpp
    type_prop/assign.cpp
    type_prop/atan.cpp
    type_prop/atanh.cpp
    type_prop/avg_pool.cpp
    type_prop/batch_norm.cpp
    type_prop/batch_to_space.cpp
    type_prop/binary_convolution.cpp
    type_prop/binary_elementwise.cpp
    type_prop/broadcast.cpp
    type_prop/bucketize.cpp
    type_prop/ceiling.cpp
    type_prop/clamp.cpp
    type_prop/concat.cpp
    type_prop/constant.cpp
    type_prop/convert.cpp
    type_prop/convert_color_i420.cpp
    type_prop/convert_color_nv12.cpp
    type_prop/convolution.cpp
    type_prop/convolution_backprop_data.cpp
    type_prop/cos.cpp
    type_prop/cosh.cpp
    type_prop/ctc_greedy_decoder.cpp
    type_prop/ctc_greedy_decoder_seq_len.cpp
    type_prop/ctc_loss.cpp
    type_prop/cum_sum.cpp
    type_prop/deformable_convolution.cpp
    type_prop/deformable_convolution_opset8.cpp
    type_prop/deformable_psroi_pooling.cpp
    type_prop/detection_output.cpp
    type_prop/depth_to_space.cpp
    type_prop/dft.cpp
    type_prop/dyn_reshape.cpp
    type_prop/einsum.cpp
    type_prop/erf.cpp
    type_prop/exp.cpp
    type_prop/experimental_detectron_generate_proposals.cpp
    type_prop/experimental_detectron_roi_feature_extractor.cpp
    type_prop/experimental_detectron_topkrois.cpp
    type_prop/strided_slice.cpp
    type_prop/elu.cpp
    type_prop/embeddingbag_offsetssum.cpp
    type_prop/experimental_detectron_detection_output.cpp
    type_prop/experimental_detectron_prior_grid_generator.cpp
    type_prop/extractimagepatches.cpp
    type_prop/embeddingbag_packedsum.cpp
    type_prop/embedding_segments_sum.cpp
    type_prop/fake_quantize.cpp
    type_prop/floor.cpp
    type_prop/floor_mod.cpp
    type_prop/framework_node.cpp
    type_prop/gather.cpp
    type_prop/gather_elements.cpp
    type_prop/gather_nd.cpp
    type_prop/gather_tree.cpp
    type_prop/gelu.cpp
    type_prop/grn.cpp
    type_prop/group_convolution.cpp
    type_prop/group_convolution_backprop_data.cpp
    type_prop/gru_cell.cpp
    type_prop/gru_sequence.cpp
    type_prop/hard_sigmoid.cpp
    type_prop/hsigmoid.cpp
    type_prop/hswish.cpp
    type_prop/idft.cpp
    type_prop/interpolate.cpp
    type_prop/logical_and.cpp
    type_prop/logical_not.cpp
    type_prop/logical_or.cpp
    type_prop/logical_xor.cpp
    type_prop/lrn.cpp
    type_prop/lstm_cell.cpp
    type_prop/lstm_sequence.cpp
    type_prop/loop.cpp
    type_prop/matmul.cpp
    type_prop/matrix_nms.cpp
    type_prop/maximum.cpp
    type_prop/max_pool.cpp
    type_prop/minimum.cpp
    type_prop/mish.cpp
    type_prop/mod.cpp
    type_prop/multiclass_nms.cpp
    type_prop/mvn.cpp
    type_prop/negative.cpp
    type_prop/non_max_suppression.cpp
    type_prop/non_zero.cpp
    type_prop/normalize_l2.cpp
    type_prop/one_hot.cpp
    type_prop/pad.cpp
    type_prop/parameter.cpp
    type_prop/power.cpp
    type_prop/prelu.cpp
    type_prop/prior_box.cpp
    type_prop/proposal.cpp
    type_prop/psroi_pooling.cpp
    type_prop/prior_box_clustered.cpp
    type_prop/random_uniform.cpp
    type_prop/range.cpp
    type_prop/read_value.cpp
    type_prop/reduce_l1.cpp
    type_prop/reduce_l2.cpp
    type_prop/reduce_logical_and.cpp
    type_prop/reduce_logical_or.cpp
    type_prop/reduce_max.cpp
    type_prop/reduce_mean.cpp
    type_prop/reduce_min.cpp
    type_prop/reduce_prod.cpp
    type_prop/reduce_sum.cpp
    type_prop/relu.cpp
    type_prop/reorg_yolo.cpp
    type_prop/reshape.cpp
    type_prop/result.cpp
    type_prop/reverse.cpp
    type_prop/reverse_sequence.cpp
    type_prop/roi_align.cpp
    type_prop/roi_pooling.cpp
    type_prop/roll.cpp
    type_prop/round.cpp
    type_prop/rnn_cell.cpp
    type_prop/rnn_sequence.cpp
    type_prop/round.cpp
    type_prop/scatter_elements_update.cpp
    type_prop/scatter_nd_update.cpp
    type_prop/scatter_update.cpp
    type_prop/select.cpp
    type_prop/selu.cpp
    type_prop/shape_of.cpp
    type_prop/shuffle_channels.cpp
    type_prop/sigmoid.cpp
    type_prop/sign.cpp
    type_prop/sin.cpp
    type_prop/sinh.cpp
    type_prop/slice.cpp
    type_prop/softmax.cpp
    type_prop/softplus.cpp
    type_prop/space_to_batch.cpp
    type_prop/space_to_depth.cpp
    type_prop/split.cpp
    type_prop/sqrt.cpp
    type_prop/squared_difference.cpp
    type_prop/squeeze.cpp
    type_prop/swish.cpp
    type_prop/tan.cpp
    type_prop/tanh.cpp
    type_prop/ti.cpp
    type_prop/tile.cpp
    type_prop/top_k.cpp
    type_prop/transpose.cpp
    type_prop/unary_elementwise.cpp
    type_prop/unsqueeze.cpp
    type_prop/variadic_split.cpp
    type_prop_layers.cpp
    visitors/partial_shape.cpp
    visitors/dimension.cpp
    visitors/user_op.cpp
    visitors/value_map.cpp
    visitors/op/acos.cpp
    visitors/op/acosh.cpp
    visitors/op/adaptive_avg_pool.cpp
    visitors/op/adaptive_max_pool.cpp
    visitors/op/add.cpp
    visitors/op/asin.cpp
    visitors/op/asinh.cpp
    visitors/op/atan.cpp
    visitors/op/atanh.cpp
    visitors/op/avg_pool.cpp
    visitors/op/batch_norm.cpp
    visitors/op/batch_to_space.cpp
    visitors/op/binary_convolution.cpp
    visitors/op/broadcast.cpp
    visitors/op/bucketize.cpp
    visitors/op/ceiling.cpp
    visitors/op/clamp.cpp
    visitors/op/constant.cpp
    visitors/op/convert.cpp
    visitors/op/convert_color_i420.cpp
    visitors/op/convert_color_nv12.cpp
    visitors/op/convolution_backprop.cpp
    visitors/op/convolution.cpp
    visitors/op/cos.cpp
    visitors/op/cosh.cpp
    visitors/op/ctc_greedy_decoder.cpp
    visitors/op/ctc_greedy_decoder_seq_len.cpp
    visitors/op/ctc_loss.cpp
    visitors/op/cum_sum.cpp
    visitors/op/deformable_convolution.cpp
    visitors/op/deformable_psroi_pooling.cpp
    visitors/op/depth_to_space.cpp
    visitors/op/detection_output.cpp
    visitors/op/dft.cpp
    visitors/op/divide.cpp
    visitors/op/einsum.cpp
    visitors/op/elu.cpp
    visitors/op/embedding_segments_sum.cpp
    visitors/op/embeddingbag_offsetssum.cpp
    visitors/op/embeddingbag_packedsum.cpp
    visitors/op/equal.cpp
    visitors/op/erf.cpp
    visitors/op/exp.cpp
    visitors/op/experimental_detectron_detection_output.cpp
    visitors/op/experimental_detectron_generate_proposals.cpp
    visitors/op/experimental_detectron_prior_grid_generator.cpp
    visitors/op/experimental_detectron_topkrois.cpp
    visitors/op/extractimagepatches.cpp
    visitors/op/fake_quantize.cpp
    visitors/op/floor_mod.cpp
    visitors/op/floor.cpp
    visitors/op/gather.cpp
    visitors/op/gather_elements.cpp
    visitors/op/gather_nd.cpp
    visitors/op/gather_tree.cpp
    visitors/op/gelu.cpp
    visitors/op/greater_equal.cpp
    visitors/op/greater.cpp
    visitors/op/grn.cpp
    visitors/op/gru_cell.cpp
    visitors/op/gru_sequence.cpp
    visitors/op/group_conv.cpp
    visitors/op/hard_sigmoid.cpp
    visitors/op/hsigmoid.cpp
    visitors/op/hswish.cpp
    visitors/op/interpolate.cpp
    visitors/op/if.cpp
    visitors/op/idft.cpp
    visitors/op/less_equal.cpp
    visitors/op/less.cpp
    visitors/op/log.cpp
    visitors/op/log_softmax.cpp
    visitors/op/logical_and.cpp
    visitors/op/logical_or.cpp
    visitors/op/logical_not.cpp
    visitors/op/logical_xor.cpp
    visitors/op/lrn.cpp
    visitors/op/lstm_cell.cpp
    visitors/op/lstm_sequence.cpp
    visitors/op/matmul.cpp
    visitors/op/matrix_nms.cpp
    visitors/op/max_pool.cpp
    visitors/op/maximum.cpp
    visitors/op/minimum.cpp
    visitors/op/mish.cpp
    visitors/op/mod.cpp
    visitors/op/multiclass_nms.cpp
    visitors/op/multiply.cpp
    visitors/op/mvn.cpp
    visitors/op/negative.cpp
    visitors/op/non_max_suppression.cpp
    visitors/op/non_zero.cpp
    visitors/op/normalize_l2.cpp
    visitors/op/not_equal.cpp
    visitors/op/one_hot.cpp
    visitors/op/pad.cpp
    visitors/op/parameter.cpp
    visitors/op/power.cpp
    visitors/op/prelu.cpp
    visitors/op/prior_box.cpp
    visitors/op/prior_box_clustered.cpp
    visitors/op/proposal.cpp
    visitors/op/psroi_pooling.cpp
    visitors/op/random_uniform.cpp
    visitors/op/reduce_l1.cpp
    visitors/op/reduce_l2.cpp
    visitors/op/reduce_logical_and.cpp
    visitors/op/reduce_logical_or.cpp
    visitors/op/reduce_max.cpp
    visitors/op/reduce_mean.cpp
    visitors/op/reduce_min.cpp
    visitors/op/reduce_prod.cpp
    visitors/op/reduce_sum.cpp
    visitors/op/region_yolo.cpp
    visitors/op/relu.cpp
    visitors/op/reorg_yolo.cpp
    visitors/op/reshape.cpp
    visitors/op/result.cpp
    visitors/op/reverse.cpp
    visitors/op/reverse_sequence.cpp
    visitors/op/rnn_cell.cpp
    visitors/op/rnn_sequence.cpp
    visitors/op/roi_pooling.cpp
    visitors/op/roll.cpp
    visitors/op/round.cpp
    visitors/op/scatter_elements_update.cpp
    visitors/op/scatter_update.cpp
    visitors/op/select.cpp
    visitors/op/space_to_depth.cpp
    visitors/op/selu.cpp
    visitors/op/shape_of.cpp
    visitors/op/shuffle_channels.cpp
    visitors/op/sigmoid.cpp
    visitors/op/sign.cpp
    visitors/op/sin.cpp
    visitors/op/sinh.cpp
    visitors/op/slice.cpp
    visitors/op/softmax.cpp
    visitors/op/softplus.cpp
    visitors/op/space_to_batch.cpp
    visitors/op/space_to_depth.cpp
    visitors/op/split.cpp
    visitors/op/sqrt.cpp
    visitors/op/squared_difference.cpp
    visitors/op/squeeze.cpp
    visitors/op/strided_slice.cpp
    visitors/op/subtract.cpp
    visitors/op/swish.cpp
    visitors/op/tan.cpp
    visitors/op/tanh.cpp
    visitors/op/topk.cpp
    visitors/op/transpose.cpp
    visitors/op/unsqueeze.cpp
    visitors/op/variadic_split.cpp
    uint4.cpp
)

# For type relaxed types
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/threading.cpp
        PROPERTIES INCLUDE_DIRECTORIES $<TARGET_PROPERTY:openvino::runtime::dev,INTERFACE_INCLUDE_DIRECTORIES>)

# Add include path to so_extension.hpp
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/extension.cpp
                    APPEND PROPERTY INCLUDE_DIRECTORIES "${OpenVINO_SOURCE_DIR}/src/core/src/")

if(SUGGEST_OVERRIDE_SUPPORTED)
    set_source_files_properties(ov_tensor_test.cpp
                                PROPERTIES COMPILE_OPTIONS -Wno-suggest-override)
endif()

list(APPEND UNIT_TESTS_DEPENDENCIES ov_template_extension)

if (ENABLE_INTEL_CPU AND ENABLE_OV_CORE_BACKEND_UNIT_TESTS)
    message(STATUS "OV_CORE_TESTS: IE:CPU enabled")
    set(ACTIVE_BACKEND_LIST ${ACTIVE_BACKEND_LIST} "IE:CPU")
    if (ENABLE_STRICT_DEPENDENCIES)
        # For convinience add a runtime dependency to build along with this target.
        # Warning: Parallel build with -GNinja may not be efficient.
        list(APPEND UNIT_TESTS_DEPENDENCIES ov_intel_cpu_plugin)
    endif()
endif()

if (ENABLE_INTEL_GPU AND ENABLE_OV_CORE_BACKEND_UNIT_TESTS)
    message(STATUS "OV_CORE_TESTS: IE:GPU enabled")
    set(ACTIVE_BACKEND_LIST ${ACTIVE_BACKEND_LIST} "IE:GPU")
    if (ENABLE_STRICT_DEPENDENCIES)
        # For convinience add a runtime dependency to build along with this target.
        # Warning: Parallel build with -GNinja may not be efficient.
        list(APPEND UNIT_TESTS_DEPENDENCIES ov_intel_gpu_plugin)
    endif()
endif()

if (ENABLE_OV_CORE_BACKEND_UNIT_TESTS)
    message(STATUS "OV_CORE_TESTS: INTERPRETER enabled")
    set(ACTIVE_BACKEND_LIST ${ACTIVE_BACKEND_LIST} INTERPRETER)
    if (ENABLE_STRICT_DEPENDENCIES)
        list(APPEND UNIT_TESTS_DEPENDENCIES ov_template_plugin)
    endif()
endif()

# backend specific test files must meet the following requirements:
# 1) The must be named <name>.in.cpp
# 2) They must be in the `test/backend` directory
# 3) Include "util/test_control.hpp" in your cpp file
# 4) add the line `static string s_manifest = "${MANIFEST}";` to your cpp file
# 5) Use the `OV_CORE_TEST` macro in place of `TEST`.
# All such files are configured via cmake which replaces all instances of cmake variables
# such as ${BACKEND_NAME} with their values, such as CPU, GPU, or INTERPRETER.

# TODO: Enable these tests in new infrastructure
set(MULTI_TEST_SRC
    backend/abc.in.cpp
    backend/interpolate.in.cpp
    backend/region_yolo.in.cpp
    backend/sqrt.in.cpp
)

set(OP_EVAL_TEST_SRC 
    # It should be a part of template plugin
    op_eval/binary_convolution.cpp
    op_eval/bucketize.cpp
    op_eval/clamp.cpp
    op_eval/einsum.cpp
    op_eval/floor_mod.cpp
    op_eval/gelu.cpp
    op_eval/hsigmoid.cpp
    op_eval/hswish.cpp
    op_eval/interpolate.cpp
    op_eval/matmul.cpp
    op_eval/memory.cpp
    op_eval/mish.cpp
    op_eval/non_zero.cpp
    op_eval/roi_align.cpp
    op_eval/roi_pooling.cpp
    op_eval/round.cpp
    op_eval/softplus.cpp
    op_eval/split.cpp
    op_eval/swish.cpp
    op_eval/strided_slice.cpp
    op_eval/transpose.cpp
    op_eval/variadic_split.cpp)

if (ENABLE_OV_ONNX_FRONTEND)
    list(APPEND MULTI_TEST_SRC
            onnx/onnx_import.in.cpp
            onnx/onnx_import_com_microsoft.in.cpp
            onnx/onnx_import_controlflow.in.cpp
            onnx/onnx_import_const_folding.in.cpp
            onnx/onnx_import_convpool.in.cpp
            onnx/onnx_import_deprecated.in.cpp
            onnx/onnx_import_dyn_shapes.in.cpp
            onnx/onnx_import_external_data.in.cpp
            onnx/onnx_import_org_openvino.in.cpp
            onnx/onnx_import_reshape.in.cpp
            onnx/onnx_import_rnn.in.cpp
            onnx/onnx_import_quant.in.cpp
            onnx/onnx_test_utils.in.cpp
            onnx/onnx_import_with_editor.in.cpp)
    list(APPEND SRC
            onnx/onnx_import_exceptions.cpp
            onnx/onnx_import_library.cpp
            onnx/onnx_tensor_names.cpp
            onnx/onnx_transformations.cpp
            onnx/onnx_editor.cpp)
endif()

# SOURCE FOR FRONTEND TESTING
file(GLOB FRONTEND_TESTS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/frontend/frontend_manager.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/frontend/decoder_transformation_extension.cpp)
list(APPEND SRC ${FRONTEND_TESTS_SRC})

foreach(src IN LISTS SRC MULTI_TEST_SRC OP_EVAL_TEST_SRC)
    if(IS_ABSOLUTE "${src}")
        list(APPEND full_src_names ${src})
    else()
        list(APPEND full_src_names "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    endif()
endforeach()
add_clang_format_target(unit-test_clang FOR_SOURCES ${full_src_names})

foreach(BACKEND_NAME ${ACTIVE_BACKEND_LIST})
    string(TOLOWER ${BACKEND_NAME} BACKEND_DIR)
    string(REGEX REPLACE "([a-z0-9]+):(.*)" "\\1" BACKEND_DIR ${BACKEND_DIR})
    set(MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/runtime/${BACKEND_DIR}/unit_test.manifest)

    foreach(TEST_SRC ${MULTI_TEST_SRC})
        string(REPLACE ":" "_" BACKEND_NAME ${BACKEND_NAME})
        string(REPLACE ".in." "_${BACKEND_NAME}." TARGET_NAME ${TEST_SRC})
        configure_file(${TEST_SRC} ${TARGET_NAME})
        set(SRC ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME} ${SRC})
    endforeach()

    message(STATUS "Adding unit test for backend ${BACKEND_NAME}")
endforeach()

set(MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/runtime/${BACKEND_DIR}/unit_test.manifest)

foreach(TEST_SRC ${OP_EVAL_TEST_SRC})
    configure_file(${TEST_SRC} ${TEST_SRC})
    set(SRC ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SRC} ${SRC})
endforeach()

add_executable(ov_core_unit_tests ${SRC})

add_dependencies(ov_core_unit_tests template_extension)

target_include_directories(ov_core_unit_tests PRIVATE ".")
target_include_directories(ov_core_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)

get_target_property(OV_CORE_SRC_DIR ngraph_obj SOURCE_DIR)
target_include_directories(ov_core_unit_tests PRIVATE ${OV_CORE_SRC_DIR}/src)

target_compile_definitions(ov_core_unit_tests
    PRIVATE
        SHARED_LIB_PREFIX="${CMAKE_SHARED_LIBRARY_PREFIX}"
        SHARED_LIB_SUFFIX="${IE_BUILD_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}")

if(UNIT_TESTS_DEPENDENCIES)
    add_dependencies(ov_core_unit_tests ${UNIT_TESTS_DEPENDENCIES})
endif()

target_link_libraries(ov_core_unit_tests PRIVATE ngraph_test_util
                                        engines_test_util
                                        ngraph_reference
                                        ngraph::builder
                                        openvino::util
                                        pugixml::static
                                        ${CMAKE_DL_LIBS}
                                        Threads::Threads
                                        openvino::conditional_compilation
                                        openvino::util
                                        frontend_common)

# Protobuf-lite does not support parsing files from prototxt format
# Since most of the onnx models are stored in this format it have to be disabled
if (ENABLE_OV_ONNX_FRONTEND)
    # It's needed by onnx_import_library.cpp and onnx_import_exceptions.cpp tests to include onnx_pb.h.
    # Not linking statically to libprotobuf (linked into libonnx) avoids false-failing onnx_editor tests.
    target_include_directories(ov_core_unit_tests
        SYSTEM PRIVATE
            $<TARGET_PROPERTY:onnx,INTERFACE_INCLUDE_DIRECTORIES>
            $<TARGET_PROPERTY:onnx_proto,INTERFACE_INCLUDE_DIRECTORIES>
            ${Protobuf_INCLUDE_DIRS})
    target_compile_definitions(ov_core_unit_tests
        PRIVATE $<TARGET_PROPERTY:onnx,INTERFACE_COMPILE_DEFINITIONS>)
    target_compile_definitions(ov_core_unit_tests PRIVATE ENABLE_OV_ONNX_FRONTEND)
endif()

if (OV_COMPILER_IS_CLANG)
    target_compile_options(ov_core_unit_tests PRIVATE -Wno-undef -Wno-reserved-id-macro)
endif()

if (ENABLE_OV_ONNX_FRONTEND)
    get_target_property(ONNX_FRONTEND_SRC_DIR ov_onnx_frontend SOURCE_DIR)
    target_include_directories(ov_core_unit_tests PRIVATE ${ONNX_FRONTEND_SRC_DIR}/src)
    target_link_libraries(ov_core_unit_tests PRIVATE ov_onnx_frontend onnx_test_util)
    if (LINUX)
        target_link_options(ov_core_unit_tests PRIVATE -Wl,--exclude-libs,ALL)
    elseif(APPLE)
        target_link_options(ov_core_unit_tests PRIVATE -Wl,-dead_strip)
    endif()
endif()

if(ENABLE_OV_IR_FRONTEND)
    add_dependencies(ov_core_unit_tests ov_ir_frontend)
endif()

install(TARGETS ov_core_unit_tests
        RUNTIME DESTINATION tests
        COMPONENT tests
        EXCLUDE_FROM_ALL)

add_subdirectory(frontend)

# process models
add_dependencies(ov_core_unit_tests test_model_zoo)

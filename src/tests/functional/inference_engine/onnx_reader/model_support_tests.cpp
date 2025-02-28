﻿// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "common_test_utils/file_utils.hpp"
#include "ie_common.h"
#include <gtest/gtest.h>
#include <fstream>

#include <ie_core.hpp>

namespace {
    std::string model_path(const char* model) {
        std::string path = ONNX_TEST_MODELS;
        path += "support_test/";
        path += model;
        return CommonTestUtils::getModelFromTestModelZoo(path);
    }
}

TEST(ONNXReader_ModelSupported, basic_model) {
    // this model is a basic ONNX model taken from ngraph's unit test (add_abc.onnx)
    // it contains the minimum number of fields required to accept this file as a valid model
    EXPECT_NO_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("supported/basic.onnx")));
}

TEST(ONNXReader_ModelSupported, basic_reverse_fields_order) {
    // this model contains the same fields as basic.onnx but serialized in reverse order
    EXPECT_NO_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("supported/basic_reverse_fields_order.onnx")));
}

TEST(ONNXReader_ModelSupported, more_fields) {
    // this model contains some optional fields (producer_name and doc_string) but 5 fields in total
    EXPECT_NO_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("supported/more_fields.onnx")));
}

TEST(ONNXReader_ModelSupported, varint_on_two_bytes) {
    // the docstring's payload length is encoded as varint using 2 bytes which should be parsed correctly
    EXPECT_NO_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("supported/varint_on_two_bytes.onnx")));
}

TEST(ONNXReader_ModelSupported, scrambled_keys) {
    // same as the prototxt_basic but with a different order of keys
    EXPECT_NO_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("supported/scrambled_keys.onnx")));
}

TEST(ONNXReader_ModelUnsupported, no_graph_field) {
    // this model contains only 2 fields (it doesn't contain a graph in particular)
    EXPECT_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("unsupported/no_graph_field.onnx")),
                 InferenceEngine::NetworkNotRead);
}

TEST(ONNXReader_ModelUnsupported, incorrect_onnx_field) {
    // in this model the second field's key is F8 (field number 31) which is doesn't exist in ONNX
    // this  test will have to be changed if the number of fields in onnx.proto
    // (ModelProto message definition) ever reaches 31 or more
    EXPECT_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("unsupported/incorrect_onnx_field.onnx")),
                 InferenceEngine::NetworkNotRead);
}

TEST(ONNXReader_ModelUnsupported, unknown_wire_type) {
    // in this model the graph key contains wire type 7 encoded in it - this value is incorrect
    EXPECT_THROW(InferenceEngine::Core{}.ReadNetwork(model_path("unsupported/unknown_wire_type.onnx")),
                 InferenceEngine::NetworkNotRead);
}

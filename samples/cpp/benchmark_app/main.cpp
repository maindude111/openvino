// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// clang-format off
#include "inference_engine.hpp"

#include "gna/gna_config.hpp"
#include "gpu/gpu_config.hpp"
#include "vpu/vpu_plugin_config.hpp"

#include "samples/args_helper.hpp"
#include "samples/common.hpp"
#include "samples/slog.hpp"

#include "benchmark_app.hpp"
#include "infer_request_wrap.hpp"
#include "inputs_filling.hpp"
#include "progress_bar.hpp"
#include "remote_blobs_filling.hpp"
#include "statistics_report.hpp"
#include "utils.hpp"
// clang-format on

using namespace InferenceEngine;

static const size_t progressBarDefaultTotalCount = 1000;

bool ParseAndCheckCommandLine(int argc, char* argv[]) {
    // ---------------------------Parsing and validating input
    // arguments--------------------------------------
    slog::info << "Parsing input parameters" << slog::endl;
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_help || FLAGS_h) {
        showUsage();
        showAvailableDevices();
        return false;
    }

    if (FLAGS_m.empty()) {
        showUsage();
        throw std::logic_error("Model is required but not set. Please set -m option.");
    }

    if (FLAGS_latency_percentile > 100 || FLAGS_latency_percentile < 1) {
        showUsage();
        throw std::logic_error("The percentile value is incorrect. The applicable values range is [1, 100].");
    }
    if (FLAGS_api != "async" && FLAGS_api != "sync") {
        throw std::logic_error("Incorrect API. Please set -api option to `sync` or `async` value.");
    }
    if (!FLAGS_hint.empty() && FLAGS_hint != "throughput" && FLAGS_hint != "tput" && FLAGS_hint != "latency") {
        throw std::logic_error("Incorrect performance hint. Please set -hint option to"
                               "either `throughput`(tput) or `latency' value.");
    }
    if (!FLAGS_report_type.empty() && FLAGS_report_type != noCntReport && FLAGS_report_type != averageCntReport &&
        FLAGS_report_type != detailedCntReport) {
        std::string err = "only " + std::string(noCntReport) + "/" + std::string(averageCntReport) + "/" +
                          std::string(detailedCntReport) +
                          " report types are supported (invalid -report_type option value)";
        throw std::logic_error(err);
    }

    if ((FLAGS_report_type == averageCntReport) && ((FLAGS_d.find("MULTI") != std::string::npos))) {
        throw std::logic_error("only " + std::string(detailedCntReport) + " report type is supported for MULTI device");
    }

    bool isNetworkCompiled = fileExt(FLAGS_m) == "blob";
    bool isPrecisionSet = !(FLAGS_ip.empty() && FLAGS_op.empty() && FLAGS_iop.empty());
    if (isNetworkCompiled && isPrecisionSet) {
        std::string err = std::string("Cannot set precision for a compiled network. ") +
                          std::string("Please re-compile your network with required precision "
                                      "using compile_tool");

        throw std::logic_error(err);
    }
    return true;
}

static void next_step(const std::string additional_info = "") {
    static size_t step_id = 0;
    static const std::map<size_t, std::string> step_names = {
        {1, "Parsing and validating input arguments"},
        {2, "Loading Inference Engine"},
        {3, "Setting device configuration"},
        {4, "Reading network files"},
        {5, "Resizing network to match image sizes and given batch"},
        {6, "Configuring input of the model"},
        {7, "Loading the model to the device"},
        {8, "Setting optimal runtime parameters"},
        {9, "Creating infer requests and preparing input blobs with data"},
        {10, "Measuring performance"},
        {11, "Dumping statistics report"}};

    step_id++;
    if (step_names.count(step_id) == 0)
        IE_THROW() << "Step ID " << step_id << " is out of total steps number " << step_names.size();

    std::cout << "[Step " << step_id << "/" << step_names.size() << "] " << step_names.at(step_id)
              << (additional_info.empty() ? "" : " (" + additional_info + ")") << std::endl;
}

/**
 * @brief The entry point of the benchmark application
 */
int main(int argc, char* argv[]) {
    std::shared_ptr<StatisticsReport> statistics;
    try {
        ExecutableNetwork exeNetwork;

        // ----------------- 1. Parsing and validating input arguments
        // -------------------------------------------------
        next_step();

        if (!ParseAndCheckCommandLine(argc, argv)) {
            return 0;
        }

        bool isNetworkCompiled = fileExt(FLAGS_m) == "blob";
        if (isNetworkCompiled) {
            slog::info << "Network is compiled" << slog::endl;
        }

        std::vector<gflags::CommandLineFlagInfo> flags;
        StatisticsReport::Parameters command_line_arguments;
        gflags::GetAllFlags(&flags);
        for (auto& flag : flags) {
            if (!flag.is_default) {
                command_line_arguments.push_back({flag.name, flag.current_value});
            }
        }
        if (!FLAGS_report_type.empty()) {
            statistics =
                std::make_shared<StatisticsReport>(StatisticsReport::Config{FLAGS_report_type, FLAGS_report_folder});
            statistics->addParameters(StatisticsReport::Category::COMMAND_LINE_PARAMETERS, command_line_arguments);
        }
        auto isFlagSetInCommandLine = [&command_line_arguments](const std::string& name) {
            return (std::find_if(command_line_arguments.begin(),
                                 command_line_arguments.end(),
                                 [name](const std::pair<std::string, std::string>& p) {
                                     return p.first == name;
                                 }) != command_line_arguments.end());
        };

        std::string device_name = FLAGS_d;

        // Parse devices
        auto devices = parseDevices(device_name);

        // Parse nstreams per device
        std::map<std::string, std::string> device_nstreams = parseNStreamsValuePerDevice(devices, FLAGS_nstreams);

        // Load device config file if specified
        std::map<std::string, std::map<std::string, std::string>> config;
#ifdef USE_OPENCV
        if (!FLAGS_load_config.empty()) {
            load_config(FLAGS_load_config, config);
        }
#endif
        /** This vector stores paths to the processed images with input names**/
        auto inputFiles = parseInputArguments(gflags::GetArgvs());

        // ----------------- 2. Loading the Inference Engine
        // -----------------------------------------------------------
        next_step();

        Core ie;

        if (FLAGS_d.find("CPU") != std::string::npos && !FLAGS_l.empty()) {
            // CPU (MKLDNN) extensions is loaded as a shared library and passed as a
            // pointer to base extension
            const auto extension_ptr = std::make_shared<InferenceEngine::Extension>(FLAGS_l);
            ie.AddExtension(extension_ptr);
            slog::info << "CPU (MKLDNN) extensions is loaded " << FLAGS_l << slog::endl;
        }

        // Load clDNN Extensions
        if ((FLAGS_d.find("GPU") != std::string::npos) && !FLAGS_c.empty()) {
            // Override config if command line parameter is specified
            if (!config.count("GPU"))
                config["GPU"] = {};
            config["GPU"][CONFIG_KEY(CONFIG_FILE)] = FLAGS_c;
        }
        if (config.count("GPU") && config.at("GPU").count(CONFIG_KEY(CONFIG_FILE))) {
            auto ext = config.at("GPU").at(CONFIG_KEY(CONFIG_FILE));
            ie.SetConfig({{CONFIG_KEY(CONFIG_FILE), ext}}, "GPU");
            slog::info << "GPU extensions is loaded " << ext << slog::endl;
        }

        slog::info << "InferenceEngine: " << GetInferenceEngineVersion() << slog::endl;
        slog::info << "Device info: " << slog::endl;
        slog::info << ie.GetVersions(device_name) << slog::endl;

        // ----------------- 3. Setting device configuration
        // -----------------------------------------------------------
        next_step();
        std::string ov_perf_hint;
        if (FLAGS_hint == "throughput" || FLAGS_hint == "tput")
            ov_perf_hint = CONFIG_VALUE(THROUGHPUT);
        else if (FLAGS_hint == "latency")
            ov_perf_hint = CONFIG_VALUE(LATENCY);

        auto getDeviceTypeFromName = [](std::string device) -> std::string {
            return device.substr(0, device.find_first_of(".("));
        };

        // Set default values from dumped config
        std::set<std::string> default_devices;
        for (auto& device : devices) {
            auto default_config = config.find(getDeviceTypeFromName(device));
            if (default_config != config.end()) {
                if (!config.count(device)) {
                    config[device] = default_config->second;
                    default_devices.emplace(default_config->first);
                }
            }
        }
        for (auto& device : default_devices) {
            config.erase(device);
        }

        bool perf_counts = false;
        // Update config per device according to command line parameters
        for (auto& device : devices) {
            if (!config.count(device))
                config[device] = {};
            std::map<std::string, std::string>& device_config = config.at(device);

            // high-level performance modes
            if (!ov_perf_hint.empty()) {
                device_config[CONFIG_KEY(PERFORMANCE_HINT)] = ov_perf_hint;
                if (FLAGS_nireq != 0)
                    device_config[CONFIG_KEY(PERFORMANCE_HINT_NUM_REQUESTS)] = std::to_string(FLAGS_nireq);
            }

            // Set performance counter
            if (isFlagSetInCommandLine("pc")) {
                // set to user defined value
                device_config[CONFIG_KEY(PERF_COUNT)] = FLAGS_pc ? CONFIG_VALUE(YES) : CONFIG_VALUE(NO);
            } else if (device_config.count(CONFIG_KEY(PERF_COUNT)) &&
                       (device_config.at(CONFIG_KEY(PERF_COUNT)) == "YES")) {
                slog::warn << "Performance counters for " << device
                           << " device is turned on. To print results use -pc option." << slog::endl;
            } else if (FLAGS_report_type == detailedCntReport || FLAGS_report_type == averageCntReport) {
                slog::warn << "Turn on performance counters for " << device << " device since report type is "
                           << FLAGS_report_type << "." << slog::endl;
                device_config[CONFIG_KEY(PERF_COUNT)] = CONFIG_VALUE(YES);
            } else if (!FLAGS_exec_graph_path.empty()) {
                slog::warn << "Turn on performance counters for " << device << " device due to execution graph dumping."
                           << slog::endl;
                device_config[CONFIG_KEY(PERF_COUNT)] = CONFIG_VALUE(YES);
            } else {
                // set to default value
                device_config[CONFIG_KEY(PERF_COUNT)] = FLAGS_pc ? CONFIG_VALUE(YES) : CONFIG_VALUE(NO);
            }
            perf_counts = (device_config.at(CONFIG_KEY(PERF_COUNT)) == CONFIG_VALUE(YES)) ? true : perf_counts;

            // the rest are individual per-device settings (overriding the values set with perf modes)
            auto setThroughputStreams = [&]() {
                const std::string key = getDeviceTypeFromName(device) + "_THROUGHPUT_STREAMS";
                if (device_nstreams.count(device)) {
                    // set to user defined value
                    std::vector<std::string> supported_config_keys =
                        ie.GetMetric(device, METRIC_KEY(SUPPORTED_CONFIG_KEYS));
                    if (std::find(supported_config_keys.begin(), supported_config_keys.end(), key) ==
                        supported_config_keys.end()) {
                        throw std::logic_error("Device " + device + " doesn't support config key '" + key + "'! " +
                                               "Please specify -nstreams for correct devices in format  "
                                               "<dev1>:<nstreams1>,<dev2>:<nstreams2>" +
                                               " or via configuration file.");
                    }
                    device_config[key] = device_nstreams.at(device);
                } else if (ov_perf_hint.empty() && !device_config.count(key) && (FLAGS_api == "async")) {
                    slog::warn << "-nstreams default value is determined automatically for " << device
                               << " device. "
                                  "Although the automatic selection usually provides a "
                                  "reasonable performance, "
                                  "but it still may be non-optimal for some cases, for more "
                                  "information look at README."
                               << slog::endl;
                    if (std::string::npos == device.find("MYRIAD"))  // MYRIAD sets the default number of
                                                                     // streams implicitly (without _AUTO)
                        device_config[key] = std::string(getDeviceTypeFromName(device) + "_THROUGHPUT_AUTO");
                }
                if (device_config.count(key))
                    device_nstreams[device] = device_config.at(key);
            };

            if (device.find("CPU") != std::string::npos) {  // CPU supports few special performance-oriented keys
                // limit threading for CPU portion of inference
                if (isFlagSetInCommandLine("nthreads"))
                    device_config[CONFIG_KEY(CPU_THREADS_NUM)] = std::to_string(FLAGS_nthreads);

                if (isFlagSetInCommandLine("enforcebf16"))
                    device_config[CONFIG_KEY(ENFORCE_BF16)] = FLAGS_enforcebf16 ? CONFIG_VALUE(YES) : CONFIG_VALUE(NO);

                if (isFlagSetInCommandLine("pin")) {
                    // set to user defined value
                    device_config[CONFIG_KEY(CPU_BIND_THREAD)] = FLAGS_pin;
                } else if (!device_config.count(CONFIG_KEY(CPU_BIND_THREAD))) {
                    if ((device_name.find("MULTI") != std::string::npos) &&
                        (device_name.find("GPU") != std::string::npos)) {
                        slog::warn << "Turn off threads pinning for " << device
                                   << " device since multi-scenario with GPU device is used." << slog::endl;
                        device_config[CONFIG_KEY(CPU_BIND_THREAD)] = CONFIG_VALUE(NO);
                    }
                }

                // for CPU execution, more throughput-oriented execution via streams
                setThroughputStreams();
            } else if (device.find("GPU") != std::string::npos) {
                // for GPU execution, more throughput-oriented execution via streams
                setThroughputStreams();

                if ((device_name.find("MULTI") != std::string::npos) &&
                    (device_name.find("CPU") != std::string::npos)) {
                    slog::warn << "Turn on GPU throttling. Multi-device execution with "
                                  "the CPU + GPU performs best with GPU throttling hint, "
                               << "which releases another CPU thread (that is otherwise "
                                  "used by the GPU driver for active polling)"
                               << slog::endl;
                    device_config[GPU_CONFIG_KEY(PLUGIN_THROTTLE)] = "1";
                }
            } else if (device.find("MYRIAD") != std::string::npos) {
                device_config[CONFIG_KEY(LOG_LEVEL)] = CONFIG_VALUE(LOG_WARNING);
                setThroughputStreams();
            } else if (device.find("GNA") != std::string::npos) {
                if (FLAGS_qb == 8)
                    device_config[GNA_CONFIG_KEY(PRECISION)] = "I8";
                else
                    device_config[GNA_CONFIG_KEY(PRECISION)] = "I16";

                if (isFlagSetInCommandLine("nthreads"))
                    device_config[GNA_CONFIG_KEY(LIB_N_THREADS)] = std::to_string(FLAGS_nthreads);
            } else {
                std::vector<std::string> supported_config_keys =
                    ie.GetMetric(device, METRIC_KEY(SUPPORTED_CONFIG_KEYS));
                auto supported = [&](const std::string& key) {
                    return std::find(std::begin(supported_config_keys), std::end(supported_config_keys), key) !=
                           std::end(supported_config_keys);
                };
                if (supported(CONFIG_KEY(CPU_THREADS_NUM)) && isFlagSetInCommandLine("nthreads")) {
                    device_config[CONFIG_KEY(CPU_THREADS_NUM)] = std::to_string(FLAGS_nthreads);
                }
                if (supported(CONFIG_KEY(CPU_THROUGHPUT_STREAMS)) && isFlagSetInCommandLine("nstreams")) {
                    device_config[CONFIG_KEY(CPU_THROUGHPUT_STREAMS)] = FLAGS_nstreams;
                }
                if (supported(CONFIG_KEY(CPU_BIND_THREAD)) && isFlagSetInCommandLine("pin")) {
                    device_config[CONFIG_KEY(CPU_BIND_THREAD)] = FLAGS_pin;
                }
            }
        }

        for (auto&& item : config) {
            ie.SetConfig(item.second, item.first);
        }

        size_t batchSize = FLAGS_b;
        Precision precision = Precision::UNSPECIFIED;
        std::string topology_name = "";
        std::vector<benchmark_app::InputsInfo> app_inputs_info;
        std::string output_name;

        // Takes priority over config from file
        if (!FLAGS_cache_dir.empty()) {
            ie.SetConfig({{CONFIG_KEY(CACHE_DIR), FLAGS_cache_dir}});
        }

        bool isDynamicNetwork = false;
        if (FLAGS_load_from_file && !isNetworkCompiled) {
            next_step();
            slog::info << "Skipping the step for loading network from file" << slog::endl;
            next_step();
            slog::info << "Skipping the step for loading network from file" << slog::endl;
            next_step();
            slog::info << "Skipping the step for loading network from file" << slog::endl;
            auto startTime = Time::now();
            exeNetwork = ie.LoadNetwork(FLAGS_m, device_name);
            auto duration_ms = double_to_string(get_duration_ms_till_now(startTime));
            slog::info << "Load network took " << duration_ms << " ms" << slog::endl;
            if (statistics)
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {{"load network time (ms)", duration_ms}});
            app_inputs_info = getInputsInfo<InputInfo::CPtr>(FLAGS_shape,
                                                             FLAGS_layout,
                                                             batchSize,
                                                             FLAGS_data_shape,
                                                             FLAGS_iscale,
                                                             FLAGS_imean,
                                                             exeNetwork.GetInputsInfo());
            if (batchSize == 0) {
                batchSize = 1;
            }
        } else if (!isNetworkCompiled) {
            // ----------------- 4. Reading the Intermediate Representation network
            // ----------------------------------------
            next_step();

            slog::info << "Loading network files" << slog::endl;

            auto startTime = Time::now();
            CNNNetwork cnnNetwork = ie.ReadNetwork(FLAGS_m);
            auto duration_ms = double_to_string(get_duration_ms_till_now(startTime));
            slog::info << "Read network took " << duration_ms << " ms" << slog::endl;
            if (statistics)
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {{"read network time (ms)", duration_ms}});

            const InputsDataMap inputInfo(cnnNetwork.getInputsInfo());
            if (inputInfo.empty()) {
                throw std::logic_error("no inputs info is provided");
            }

            // ----------------- 5. Resizing network to match image sizes and given
            // batch ----------------------------------
            next_step();
            // Parse input shapes if specified
            bool reshape = false;
            app_inputs_info = getInputsInfo<InputInfo::Ptr>(FLAGS_shape,
                                                            FLAGS_layout,
                                                            FLAGS_b,
                                                            FLAGS_data_shape,
                                                            FLAGS_iscale,
                                                            FLAGS_imean,
                                                            inputInfo,
                                                            reshape);
            if (reshape) {
                benchmark_app::PartialShapes shapes = {};
                for (auto& item : app_inputs_info[0])
                    shapes[item.first] = item.second.partialShape;
                slog::info << "Reshaping network: " << getShapesString(shapes) << slog::endl;
                startTime = Time::now();
                cnnNetwork.reshape(shapes);
                duration_ms = double_to_string(get_duration_ms_till_now(startTime));
                slog::info << "Reshape network took " << duration_ms << " ms" << slog::endl;
                if (statistics)
                    statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                              {{"reshape network time (ms)", duration_ms}});
            }
            topology_name = cnnNetwork.getName();

            // Check if network has dynamic shapes
            auto input_info = app_inputs_info[0];
            isDynamicNetwork = std::any_of(input_info.begin(),
                                           input_info.end(),
                                           [](const std::pair<std::string, benchmark_app::InputInfo>& i) {
                                               return i.second.partialShape.is_dynamic();
                                           });

            // use batch size according to provided layout and shapes (static case)
            if (batchSize == 0 || !isDynamicNetwork) {
                batchSize = (!FLAGS_layout.empty()) ? getBatchSize(app_inputs_info[0]) : cnnNetwork.getBatchSize();
            }

            slog::info << (batchSize != 0 ? "Network batch size was changed to: " : "Network batch size: ") << batchSize
                       << slog::endl;

            // ----------------- 6. Configuring inputs and outputs
            // ----------------------------------------------------------------------
            next_step();

            processPrecision(cnnNetwork, FLAGS_ip, FLAGS_op, FLAGS_iop);
            for (auto& item : cnnNetwork.getInputsInfo()) {
                // if precision for input set by user, then set it to app_inputs
                // if it an image, set U8
                if (!FLAGS_ip.empty() || FLAGS_iop.find(item.first) != std::string::npos ||
                    item.second->getPartialShape().is_dynamic()) {
                    app_inputs_info[0].at(item.first).precision = item.second->getPrecision();
                } else if (app_inputs_info[0].at(item.first).isImage()) {
                    app_inputs_info[0].at(item.first).precision = Precision::U8;
                    item.second->setPrecision(app_inputs_info[0].at(item.first).precision);
                }
            }

            printInputAndOutputsInfo(cnnNetwork);
            // ----------------- 7. Loading the model to the device
            // --------------------------------------------------------
            next_step();
            startTime = Time::now();
            exeNetwork = ie.LoadNetwork(cnnNetwork, device_name);
            duration_ms = double_to_string(get_duration_ms_till_now(startTime));
            slog::info << "Load network took " << duration_ms << " ms" << slog::endl;
            if (statistics)
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {{"load network time (ms)", duration_ms}});
        } else {
            next_step();
            slog::info << "Skipping the step for compiled network" << slog::endl;
            next_step();
            slog::info << "Skipping the step for compiled network" << slog::endl;
            next_step();
            slog::info << "Skipping the step for compiled network" << slog::endl;
            // ----------------- 7. Loading the model to the device
            // --------------------------------------------------------
            next_step();
            auto startTime = Time::now();
            exeNetwork = ie.ImportNetwork(FLAGS_m, device_name, {});
            auto duration_ms = double_to_string(get_duration_ms_till_now(startTime));
            slog::info << "Import network took " << duration_ms << " ms" << slog::endl;
            if (statistics)
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {{"import network time (ms)", duration_ms}});
            app_inputs_info = getInputsInfo<InputInfo::CPtr>(FLAGS_shape,
                                                             FLAGS_layout,
                                                             FLAGS_b,
                                                             FLAGS_data_shape,
                                                             FLAGS_iscale,
                                                             FLAGS_imean,
                                                             exeNetwork.GetInputsInfo());
            if (batchSize == 0) {
                batchSize = 1;
            }
        }

        if (isDynamicNetwork && FLAGS_api == "sync") {
            throw std::logic_error("Benchmarking of the model with dynamic shapes is available for async API only."
                                   "Please use -api async -nstreams 1 -nireq 1 to emulate sync behavior");
        }

        // Defining of benchmark mode
        // for static models inference only mode is used as default one
        bool inferenceOnly = FLAGS_inference_only;
        if (isDynamicNetwork) {
            if (isFlagSetInCommandLine("inference_only") && inferenceOnly && app_inputs_info.size() != 1) {
                throw std::logic_error(
                    "Dynamic models with different input data shapes must be benchmarked only in full mode.");
            }
            inferenceOnly = isFlagSetInCommandLine("inference_only") && inferenceOnly && app_inputs_info.size() == 1;
        }

        // ----------------- 8. Querying optimal runtime parameters
        // -----------------------------------------------------
        next_step();
        // output of the actual settings that the device selected based on the hint
        if (!ov_perf_hint.empty()) {
            for (const auto& device : devices) {
                std::vector<std::string> supported_config_keys =
                    ie.GetMetric(device, METRIC_KEY(SUPPORTED_CONFIG_KEYS));
                slog::info << "Device: " << device << slog::endl;
                for (const auto& cfg : supported_config_keys) {
                    try {
                        slog::info << "  {" << cfg << " , " << exeNetwork.GetConfig(cfg).as<std::string>();
                    } catch (...) {
                    };
                    slog::info << " }" << slog::endl;
                }
            }
        }

        // Update number of streams
        for (auto&& ds : device_nstreams) {
            const std::string key = getDeviceTypeFromName(ds.first) + "_THROUGHPUT_STREAMS";
            device_nstreams[ds.first] = ie.GetConfig(ds.first, key).as<std::string>();
        }

        // Number of requests
        uint32_t nireq = FLAGS_nireq;
        if (nireq == 0) {
            if (FLAGS_api == "sync") {
                nireq = 1;
            } else {
                std::string key = METRIC_KEY(OPTIMAL_NUMBER_OF_INFER_REQUESTS);
                try {
                    nireq = exeNetwork.GetMetric(key).as<unsigned int>();
                } catch (const std::exception& ex) {
                    IE_THROW() << "Every device used with the benchmark_app should "
                               << "support OPTIMAL_NUMBER_OF_INFER_REQUESTS "
                                  "ExecutableNetwork metric. "
                               << "Failed to query the metric for the " << device_name << " with error:" << ex.what();
                }
            }
        }

        // Iteration limit
        uint32_t niter = FLAGS_niter;
        size_t shape_groups_num = app_inputs_info.size();
        if ((niter > 0) && (FLAGS_api == "async")) {
            if (shape_groups_num > nireq) {
                niter = ((niter + shape_groups_num - 1) / shape_groups_num) * shape_groups_num;
                if (FLAGS_niter != niter) {
                    slog::warn << "Number of iterations was aligned by data shape groups number from " << FLAGS_niter
                               << " to " << niter << " using number of possible input shapes " << shape_groups_num
                               << slog::endl;
                }
            } else {
                niter = ((niter + nireq - 1) / nireq) * nireq;
                if (FLAGS_niter != niter) {
                    slog::warn << "Number of iterations was aligned by request number from " << FLAGS_niter << " to "
                               << niter << " using number of requests " << nireq << slog::endl;
                }
            }
        }

        // Time limit
        uint32_t duration_seconds = 0;
        if (FLAGS_t != 0) {
            // time limit
            duration_seconds = FLAGS_t;
        } else if (FLAGS_niter == 0) {
            // default time limit
            duration_seconds = deviceDefaultDeviceDurationInSeconds(device_name);
        }
        uint64_t duration_nanoseconds = getDurationInNanoseconds(duration_seconds);

        if (statistics) {
            statistics->addParameters(
                StatisticsReport::Category::RUNTIME_CONFIG,
                {
                    {"benchmark mode", inferenceOnly ? "inference only" : "full"},
                    {"topology", topology_name},
                    {"target device", device_name},
                    {"API", FLAGS_api},
                    {"precision", std::string(precision.name())},
                    {"batch size", std::to_string(batchSize)},
                    {"number of iterations", std::to_string(niter)},
                    {"number of parallel infer requests", std::to_string(nireq)},
                    {"duration (ms)", std::to_string(getDurationInMilliseconds(duration_seconds))},
                });
            for (auto& nstreams : device_nstreams) {
                std::stringstream ss;
                ss << "number of " << nstreams.first << " streams";
                statistics->addParameters(StatisticsReport::Category::RUNTIME_CONFIG,
                                          {
                                              {ss.str(), nstreams.second},
                                          });
            }
        }

        // ----------------- 9. Creating infer requests and filling input blobs
        // ----------------------------------------
        next_step();

        InferRequestsQueue inferRequestsQueue(exeNetwork, nireq, app_inputs_info.size(), FLAGS_pcseq);

        bool inputHasName = false;
        if (inputFiles.size() > 0) {
            inputHasName = inputFiles.begin()->first != "";
        }
        bool newInputType = isDynamicNetwork || inputHasName;
        // create vector to store remote input blobs buffer
        std::vector<::gpu::BufferType> clInputsBuffer;
        bool useGpuMem = false;

        std::map<std::string, std::vector<InferenceEngine::Blob::Ptr>> inputsData;
        if (isFlagSetInCommandLine("use_device_mem")) {
            if (device_name.find("GPU") == 0) {
                inputsData = ::gpu::getRemoteInputBlobs(inputFiles, app_inputs_info, exeNetwork, clInputsBuffer);
                useGpuMem = true;
            } else if (device_name.find("CPU") == 0) {
                if (newInputType) {
                    inputsData = getBlobs(inputFiles, app_inputs_info);
                } else {
                    inputsData =
                        getBlobsStaticCase(inputFiles.empty() ? std::vector<std::string>{} : inputFiles.begin()->second,
                                           batchSize,
                                           app_inputs_info[0],
                                           nireq);
                }
            } else {
                IE_THROW() << "Requested device doesn't support `use_device_mem` option.";
            }
        } else {
            if (newInputType) {
                inputsData = getBlobs(inputFiles, app_inputs_info);
            } else {
                inputsData =
                    getBlobsStaticCase(inputFiles.empty() ? std::vector<std::string>{} : inputFiles.begin()->second,
                                       batchSize,
                                       app_inputs_info[0],
                                       nireq);
            }
        }
        // ----------------- 10. Measuring performance
        // ------------------------------------------------------------------
        size_t progressCnt = 0;
        size_t progressBarTotalCount = progressBarDefaultTotalCount;
        size_t iteration = 0;

        std::stringstream ss;
        ss << "Start inference " << FLAGS_api << "hronously";
        if (FLAGS_api == "async") {
            if (!ss.str().empty()) {
                ss << ", ";
            }
            ss << nireq << " inference requests";
            std::stringstream device_ss;
            for (auto& nstreams : device_nstreams) {
                if (!device_ss.str().empty()) {
                    device_ss << ", ";
                }
                device_ss << nstreams.second << " streams for " << nstreams.first;
            }
            if (!device_ss.str().empty()) {
                ss << " using " << device_ss.str();
            }
        }
        ss << ", limits: ";
        if (duration_seconds > 0) {
            ss << getDurationInMilliseconds(duration_seconds) << " ms duration";
        }
        if (niter != 0) {
            if (duration_seconds == 0) {
                progressBarTotalCount = niter;
            }
            if (duration_seconds > 0) {
                ss << ", ";
            }
            ss << niter << " iterations";
        }

        next_step(ss.str());

        if (inferenceOnly) {
            slog::info << "BENCHMARK IS IN INFERENCE ONLY MODE." << slog::endl;
            slog::info << "Input blobs will be filled once before performance measurements." << slog::endl;
        } else {
            slog::info << "BENCHMARK IS IN FULL MODE." << slog::endl;
            slog::info << "Inputs setup stage will be included in performance measurements." << slog::endl;
        }

        // copy prepared data straight into inferRequest->getBlob()
        // for inference only mode
        if (inferenceOnly) {
            if (nireq < inputsData.begin()->second.size())
                slog::warn << "Only " << nireq << " test configs will be used." << slog::endl;
            size_t i = 0;
            for (auto& inferRequest : inferRequestsQueue.requests) {
                auto inputs = app_inputs_info[i % app_inputs_info.size()];
                for (auto& item : inputs) {
                    auto inputName = item.first;
                    const auto& inputBlob = inputsData.at(inputName)[i % inputsData.at(inputName).size()];
                    // for remote blobs setBlob is used, they are already allocated on the device
                    if (useGpuMem) {
                        inferRequest->setBlob(inputName, inputBlob);
                    } else {
                        InferenceEngine::Blob::Ptr requestBlob = inferRequest->getBlob(inputName);
                        if (isDynamicNetwork) {
                            requestBlob->setShape(inputBlob->getTensorDesc().getDims());
                        }
                        copyBlobData(requestBlob, inputBlob);
                    }
                }

                if (useGpuMem) {
                    auto outputBlobs = ::gpu::getRemoteOutputBlobs(exeNetwork, inferRequest->getOutputClBuffer());
                    for (auto& output : exeNetwork.GetOutputsInfo()) {
                        inferRequest->setBlob(output.first, outputBlobs[output.first]);
                    }
                }
                ++i;
            }
        }

        // warming up - out of scope
        auto inferRequest = inferRequestsQueue.getIdleRequest();
        if (!inferRequest) {
            IE_THROW() << "No idle Infer Requests!";
        }

        if (!inferenceOnly) {
            auto inputs = app_inputs_info[0];

            for (auto& item : inputs) {
                auto inputName = item.first;
                const auto& data = inputsData.at(inputName)[0];
                inferRequest->setBlob(inputName, data);
            }

            if (useGpuMem) {
                auto outputBlobs = ::gpu::getRemoteOutputBlobs(exeNetwork, inferRequest->getOutputClBuffer());
                for (auto& output : exeNetwork.GetOutputsInfo()) {
                    inferRequest->setBlob(output.first, outputBlobs[output.first]);
                }
            }
        }

        if (FLAGS_api == "sync") {
            inferRequest->infer();
        } else {
            inferRequest->startAsync();
        }

        inferRequestsQueue.waitAll();

        auto duration_ms = double_to_string(inferRequestsQueue.getLatencies()[0]);
        slog::info << "First inference took " << duration_ms << " ms" << slog::endl;

        if (statistics) {
            statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                      {{"first inference time (ms)", duration_ms}});
        }
        inferRequestsQueue.resetTimes();

        size_t processedFramesN = 0;
        auto startTime = Time::now();
        auto execTime = std::chrono::duration_cast<ns>(Time::now() - startTime).count();

        /** Start inference & calculate performance **/
        /** to align number if iterations to guarantee that last infer requests are
         * executed in the same conditions **/
        ProgressBar progressBar(progressBarTotalCount, FLAGS_stream_output, FLAGS_progress);
        while ((niter != 0LL && iteration < niter) ||
               (duration_nanoseconds != 0LL && (uint64_t)execTime < duration_nanoseconds) ||
               (FLAGS_api == "async" && iteration % nireq != 0)) {
            inferRequest = inferRequestsQueue.getIdleRequest();
            if (!inferRequest) {
                IE_THROW() << "No idle Infer Requests!";
            }

            if (!inferenceOnly) {
                auto inputs = app_inputs_info[iteration % app_inputs_info.size()];

                if (FLAGS_pcseq) {
                    inferRequest->setLatencyGroupId(iteration % app_inputs_info.size());
                }

                if (isDynamicNetwork) {
                    batchSize = getBatchSize(inputs);
                }

                for (auto& item : inputs) {
                    auto inputName = item.first;
                    const auto& data = inputsData.at(inputName)[iteration % inputsData.at(inputName).size()];
                    inferRequest->setBlob(inputName, data);
                }

                if (useGpuMem) {
                    auto outputBlobs = ::gpu::getRemoteOutputBlobs(exeNetwork, inferRequest->getOutputClBuffer());
                    for (auto& output : exeNetwork.GetOutputsInfo()) {
                        inferRequest->setBlob(output.first, outputBlobs[output.first]);
                    }
                }
            }

            if (FLAGS_api == "sync") {
                inferRequest->infer();
            } else {
                // As the inference request is currently idle, the wait() adds no
                // additional overhead (and should return immediately). The primary
                // reason for calling the method is exception checking/re-throwing.
                // Callback, that governs the actual execution can handle errors as
                // well, but as it uses just error codes it has no details like ‘what()’
                // method of `std::exception` So, rechecking for any exceptions here.
                inferRequest->wait();
                inferRequest->startAsync();
            }
            ++iteration;

            execTime = std::chrono::duration_cast<ns>(Time::now() - startTime).count();
            processedFramesN += batchSize;

            if (niter > 0) {
                progressBar.addProgress(1);
            } else {
                // calculate how many progress intervals are covered by current
                // iteration. depends on the current iteration time and time of each
                // progress interval. Previously covered progress intervals must be
                // skipped.
                auto progressIntervalTime = duration_nanoseconds / progressBarTotalCount;
                size_t newProgress = execTime / progressIntervalTime - progressCnt;
                progressBar.addProgress(newProgress);
                progressCnt += newProgress;
            }
        }

        // wait the latest inference executions
        inferRequestsQueue.waitAll();

        LatencyMetrics generalLatency(inferRequestsQueue.getLatencies());
        std::vector<LatencyMetrics> groupLatencies = {};
        if (FLAGS_pcseq && app_inputs_info.size() > 1) {
            for (auto lats : inferRequestsQueue.getLatencyGroups()) {
                groupLatencies.push_back(LatencyMetrics(lats));
            }
        }

        double totalDuration = inferRequestsQueue.getDurationInMilliseconds();
        double fps = (FLAGS_api == "sync") ? batchSize * 1000.0 / generalLatency.percentile(FLAGS_latency_percentile)
                                           : 1000.0 * processedFramesN / totalDuration;

        if (statistics) {
            statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                      {
                                          {"total execution time (ms)", double_to_string(totalDuration)},
                                          {"total number of iterations", std::to_string(iteration)},
                                      });
            if (device_name.find("MULTI") == std::string::npos) {
                std::string latency_label;
                if (FLAGS_latency_percentile == 50) {
                    latency_label = "Median latency (ms)";
                } else {
                    latency_label = "latency (" + std::to_string(FLAGS_latency_percentile) + " percentile) (ms)";
                }
                statistics->addParameters(
                    StatisticsReport::Category::EXECUTION_RESULTS,
                    {
                        {latency_label, double_to_string(generalLatency.percentile(FLAGS_latency_percentile))},
                    });
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {
                                              {"Average latency (ms)", double_to_string(generalLatency.average())},
                                          });
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {
                                              {"Min latency (ms)", double_to_string(generalLatency.min())},
                                          });
                statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                          {
                                              {"Max latency (ms)", double_to_string(generalLatency.max())},
                                          });

                if (FLAGS_pcseq && app_inputs_info.size() > 1) {
                    statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                              {
                                                  {"Latency for each data shape group:", ""},
                                              });
                    for (size_t i = 0; i < app_inputs_info.size(); ++i) {
                        std::string data_shapes_string = "";
                        data_shapes_string += std::to_string(i + 1) + ". ";
                        for (auto& item : app_inputs_info[i]) {
                            data_shapes_string += item.first + " : " + getShapeString(item.second.dataShape) + " ";
                        }
                        statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                                  {
                                                      {data_shapes_string, ""},
                                                  });
                        statistics->addParameters(
                            StatisticsReport::Category::EXECUTION_RESULTS,
                            {
                                {latency_label,
                                 double_to_string(groupLatencies[i].percentile(FLAGS_latency_percentile))},
                            });
                        statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                                  {
                                                      {"Average (ms)", double_to_string(groupLatencies[i].average())},
                                                  });
                        statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                                  {
                                                      {"Min (ms)", double_to_string(groupLatencies[i].min())},
                                                  });
                        statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                                  {
                                                      {"Max (ms)", double_to_string(groupLatencies[i].max())},
                                                  });
                    }
                }
            }
            statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                      {{"throughput", double_to_string(fps)}});
        }
        progressBar.finish();

        // ----------------- 11. Dumping statistics report
        // -------------------------------------------------------------
        next_step();

#ifdef USE_OPENCV
        if (!FLAGS_dump_config.empty()) {
            dump_config(FLAGS_dump_config, config);
            slog::info << "Inference Engine configuration settings were dumped to " << FLAGS_dump_config << slog::endl;
        }
#endif

        if (!FLAGS_exec_graph_path.empty()) {
            try {
                CNNNetwork execGraphInfo = exeNetwork.GetExecGraphInfo();
                execGraphInfo.serialize(FLAGS_exec_graph_path);
                slog::info << "executable graph is stored to " << FLAGS_exec_graph_path << slog::endl;
            } catch (const std::exception& ex) {
                slog::err << "Can't get executable graph: " << ex.what() << slog::endl;
            }
        }

        if (perf_counts) {
            std::vector<std::map<std::string, InferenceEngine::InferenceEngineProfileInfo>> perfCounts;
            for (size_t ireq = 0; ireq < nireq; ireq++) {
                auto reqPerfCounts = inferRequestsQueue.requests[ireq]->getPerformanceCounts();
                if (FLAGS_pc) {
                    slog::info << "Performance counts for " << ireq << "-th infer request:" << slog::endl;
                    printPerformanceCounts(reqPerfCounts, std::cout, getFullDeviceName(ie, FLAGS_d), false);
                }
                perfCounts.push_back(reqPerfCounts);
            }
            if (statistics) {
                statistics->dumpPerformanceCounters(perfCounts);
            }
        }

        if (statistics)
            statistics->dump();

        // Performance metrics report
        slog::info << "Count:      " << iteration << " iterations" << slog::endl;
        slog::info << "Duration:   " << double_to_string(totalDuration) << " ms" << slog::endl;
        if (device_name.find("MULTI") == std::string::npos) {
            slog::info << "Latency: " << slog::endl;
            generalLatency.logTotal(FLAGS_latency_percentile);

            if (FLAGS_pcseq && app_inputs_info.size() > 1) {
                slog::info << "Latency for each data shape group:" << slog::endl;
                for (size_t i = 0; i < app_inputs_info.size(); ++i) {
                    slog::info << (i + 1) << ".";
                    for (auto& item : app_inputs_info[i]) {
                        std::stringstream input_shape;
                        auto shape = item.second.dataShape;
                        std::copy(shape.begin(), shape.end() - 1, std::ostream_iterator<int>(input_shape, ","));
                        input_shape << shape.back();
                        slog::info << " " << item.first << " : " << getShapeString(item.second.dataShape);
                    }
                    slog::info << slog::endl;

                    groupLatencies[i].logTotal(FLAGS_latency_percentile);
                }
            }
        }
        slog::info << "Throughput: " << double_to_string(fps) << " FPS" << slog::endl;

    } catch (const std::exception& ex) {
        slog::err << ex.what() << slog::endl;

        if (statistics) {
            statistics->addParameters(StatisticsReport::Category::EXECUTION_RESULTS,
                                      {
                                          {"error", ex.what()},
                                      });
            statistics->dump();
        }

        return 3;
    }

    return 0;
}

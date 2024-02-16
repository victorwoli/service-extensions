/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// A dynamic test runner for Wasm plugins. Given a proto specification, this
// binary provides various inputs to a ProxyWasm plugin for each test, and
// validates a configured set of expectations about output and side effects.
//
// TODO Future features
// - Benchmarking incl. init plugin (root context) and stream (http context)
// - YAML config input support (--yaml instead of --proto)
// - Structured output (JSON) rather than stdout/stderr
// - Publish test runner as Docker image
// - Crash / death tests?

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "re2/re2.h"
#include "test/dynamic_test.h"
#include "test/framework.h"
#include "test/runner.pb.h"

ABSL_FLAG(std::string, proto, "", "Path to test config. Required.");
ABSL_FLAG(std::string, plugin, "", "Override path to plugin wasm.");
ABSL_FLAG(std::string, config, "", "Override path to plugin config.");
ABSL_FLAG(service_extensions_samples::pb::Runtime::LogLevel, min_log_level,
          service_extensions_samples::pb::Runtime::UNDEFINED,
          "Override min_log_level.");

namespace service_extensions_samples {

// Proto enum flag (de)serialization.
namespace pb {
bool AbslParseFlag(absl::string_view text, pb::Runtime::LogLevel* ll,
                   std::string* error) {
  return Runtime::LogLevel_Parse(std::string(text), ll);
}
std::string AbslUnparseFlag(pb::Runtime::LogLevel ll) {
  return Runtime_LogLevel_Name(ll);
}
}  // namespace pb

absl::StatusOr<pb::TestSuite> ParseInputs(int argc, char** argv) {
  auto params = absl::ParseCommandLine(argc, argv);

  // Parse test config.
  std::string cfg_path = absl::GetFlag(FLAGS_proto);
  if (cfg_path.empty()) {
    return absl::InvalidArgumentError("Flag --proto is required.");
  }
  auto cfg_bytes = ReadDataFile(cfg_path);
  if (!cfg_bytes.ok()) {
    return cfg_bytes.status();
  }
  pb::TestSuite tests;
  if (!google::protobuf::TextFormat::ParseFromString(*cfg_bytes, &tests)) {
    return absl::InvalidArgumentError("Failed to parse input proto");
  }
  // Apply flag overrides.
  std::string plugin_override = absl::GetFlag(FLAGS_plugin);
  std::string config_override = absl::GetFlag(FLAGS_config);
  pb::Runtime::LogLevel mll_override = absl::GetFlag(FLAGS_min_log_level);
  if (!plugin_override.empty()) {
    tests.mutable_runtime()->set_wasm_path(plugin_override);
  }
  if (!config_override.empty()) {
    tests.mutable_runtime()->set_config_path(config_override);
  }
  if (pb::Runtime::LogLevel_IsValid(mll_override)) {
    tests.mutable_runtime()->set_min_log_level(mll_override);
  }
  if (tests.runtime().min_log_level() == pb::Runtime::TRACE) {
    std::cout << "TRACE from runner: final config:\n" << tests.DebugString();
  }
  return tests;
}

absl::Status RunTests(const pb::TestSuite& cfg) {
  // Run functional tests.
  for (const auto& engine : proxy_wasm::getWasmEngines()) {
    for (const auto& test : cfg.test()) {
      testing::RegisterTest(
          absl::StrCat("HttpTest_", engine).c_str(), test.name().c_str(),
          nullptr, nullptr, __FILE__, __LINE__,
          // Important to use the fixture type as the return type here.
          [=]() -> DynamicFixture* {
            return new DynamicTest(engine, cfg.runtime(), test);
          });
    }
  }
  bool tests_ok = RUN_ALL_TESTS() == 0;

  /*
  // Run performance benchmarks.
  auto BM_test = [](benchmark::State& st, auto Inputs) {  };
  benchmark::RegisterBenchmark(test_input.name(), BM_test, test_input);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  */

  return tests_ok ? absl::OkStatus() : absl::UnknownError("tests failed");
}

absl::Status main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // TODO benchmark::Initialize(&argc, argv);

  auto cfg = ParseInputs(argc, argv);
  if (!cfg.ok()) {
    return cfg.status();
  }

  return RunTests(*cfg);
}

}  // namespace service_extensions_samples

int main(int argc, char** argv) {
  absl::Status res = service_extensions_samples::main(argc, argv);
  if (!res.ok()) {
    std::cerr << res << std::endl;
    return 1;
  }
  return 0;
}

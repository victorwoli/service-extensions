# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_rust//crate_universe:defs.bzl", "crates_vendor")

exports_files(
    [
        "Cargo.toml",
        "Cargo.Bazel.lock",
    ],
)

# Run this target to regenerate cargo_lockfile and vendor_path/*.
# $ bazelisk run bazel/cargo:crates_vendor -- --repin
crates_vendor(
    name = "crates_vendor",
    cargo_lockfile = ":Cargo.Bazel.lock",
    generate_target_compatible_with = False,
    manifests = [":Cargo.toml"],
    mode = "remote",
    repository_name = "cu",  # shorten generated paths for Windows...
    tags = ["manual"],
    vendor_path = "remote",
    supported_platform_triples = ["x86_64-unknown-linux-gnu", "wasm32-wasi"], # needed x86_64-unknown-linux-gnu to fix issue with getrandom building for wrong environment
)
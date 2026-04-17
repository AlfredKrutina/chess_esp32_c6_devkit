// Copyright 2024 The ODML Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_ODML_INFRA_GENAI_INFERENCE_C_LLM_INFERENCE_ENGINE_IOS_H_
#define THIRD_PARTY_ODML_INFRA_GENAI_INFERENCE_C_LLM_INFERENCE_ENGINE_IOS_H_

#import <CoreGraphics/CoreGraphics.h>

#ifndef ODML_EXPORT
#define ODML_EXPORT __attribute__((visibility("default")))
#endif  // ODML_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

typedef void LlmInferenceEngine_Session;

// Adds an CGImage to the session.
ODML_EXPORT int LlmInferenceEngine_Session_AddCgImage(
    LlmInferenceEngine_Session* session, CGImageRef cg_image_ref,
    char** error_msg);

#ifdef __cplusplus
}  // extern C
#endif

#endif  // THIRD_PARTY_ODML_INFRA_GENAI_INFERENCE_C_LLM_INFERENCE_ENGINE_IOS_H_

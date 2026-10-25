# Native qualification record

This is a qualification/audit record, not a native benchmark receipt. The JSON payload below retains the reviewed values. Original JSON payload SHA-256: `20158880BD7116086E7139C514AC2652D120C48A74A4BB897EB53F7525A6A3F6`.

```json
{
  "schema": "dxui-hosted-native-public-summary-v1",
  "event": "pull_request",
  "workflowRun": 40222621151,
  "reviewedHead": "2fed82465dcee9f213753754e0e8e54f353cf38f",
  "reviewedTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "mergedMainCommit": "97b26bad7fcdb0aa3dbb2efd4bceac77aab5a295",
  "mergedMainTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "compiledSourceCommit": "9ac8cc0bb857a0a71f0ea0e94ef3a51c69be7371",
  "compiledTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "compiledTreeMatchesReviewedAndMergedTree": true,
  "runConclusion": null,
  "sixNativeProfilesPassed": true,
  "nativeArtifactCount": 6,
  "allProviderArchiveDigestsMatched": true,
  "artifacts": [
    {
      "profile": "ARM64-ASan-Debug",
      "archiveSha256": "D84658DB6D23FE40A089A259C6B1B65B382069305B2B594FD9E5507D1A08A18D",
      "archiveBytes": 2947619,
      "zipEntries": 122,
      "expandedFiles": 122,
      "providerDigestMatched": true
    },
    {
      "profile": "ARM64-Release",
      "archiveSha256": "29F1B2BA1D51CEF1247E6B196D33CD618132DADA23514DADDC98836E32BFD2F4",
      "archiveBytes": 2927692,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-ASan-Debug",
      "archiveSha256": "02949BF76FB1C096F15962661E4C31086120206FBB59E73C4714220471436BC1",
      "archiveBytes": 2946134,
      "zipEntries": 122,
      "expandedFiles": 122,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-Debug",
      "archiveSha256": "516A63702B2C1BB522E86A6D28B0D30F100D2B319450899F4B03A5F807F244C3",
      "archiveBytes": 2930668,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-Release",
      "archiveSha256": "E84B0F1322BA75CB182F0F913DC44AABD7414462FBB275CD16F9FEB7BAEE2C0C",
      "archiveBytes": 3068179,
      "zipEntries": 134,
      "expandedFiles": 134,
      "providerDigestMatched": true
    },
    {
      "profile": "ARM64-Debug",
      "archiveSha256": "18D2A22B59AEF45CF510F7F33EC9A0CF15ED06F4BA341BD09A838A48D7E0422C",
      "archiveBytes": 2939011,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    }
  ],
  "profiles": [
    {
      "profile": "native-ARM64-ASan-Debug",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 90,
          "dataStreamsHashed": 84,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 2,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [
        {
          "expectedCanaryDetected": true,
          "expectedCanaryExitCode": 1,
          "reportSha256": "437CA478F01BAB6D52093CCD59C03C6DD066E00FD30214336BC936DE974C5863"
        }
      ],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    },
    {
      "profile": "native-ARM64-Debug",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 88,
          "dataStreamsHashed": 82,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 1,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    },
    {
      "profile": "native-ARM64-Release",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 88,
          "dataStreamsHashed": 82,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 1,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    },
    {
      "profile": "native-x64-ASan-Debug",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 90,
          "dataStreamsHashed": 84,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 2,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [
        {
          "expectedCanaryDetected": true,
          "expectedCanaryExitCode": 1,
          "reportSha256": "BCA9EE77A5B22677EA50E9ECF70B065FE675E6830A718A1F6DA609F9985835FA"
        }
      ],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    },
    {
      "profile": "native-x64-Debug",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 88,
          "dataStreamsHashed": 82,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 1,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    },
    {
      "profile": "native-x64-Release",
      "testRuns": [
        {
          "result": "passed",
          "inventoryEntries": 88,
          "dataStreamsHashed": 82,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 21,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        },
        {
          "result": "passed",
          "inventoryEntries": 12,
          "dataStreamsHashed": 9,
          "payloadHashMismatches": 0,
          "missingPayloadFiles": 0,
          "suiteReports": 2,
          "failedSuiteReports": 0,
          "skippedSuites": 0
        }
      ],
      "externalConsumerRuns": 1,
      "externalConsumerRunsPassed": true,
      "negativeChecksPerConsumer": 10,
      "nativeModuleChecksPerConsumer": 12,
      "gallery": {
        "allRecordedGalleryRunsPassed": true,
        "recordedRuns": 1
      },
      "asanCanary": [],
      "interactiveMenu": [
        {
          "result": "passed",
          "interactiveLeaseCompleted": true,
          "failedTraceSummaries": 0
        }
      ]
    }
  ],
  "limits": [
    "Six hosted native jobs passed; the PR workflow itself remained in progress while paired benchmarking was pending at capture. A direct workflow-dispatch run completed successfully with the paired job skipped by its event type.",
    "ARM64 profiles are hosted native execution, distinct from local ARM64 cross-builds.",
    "The intentional ASan use-after-free canary was detected separately from the passing ASan Debug suite.",
    "Hosted ARM64 Debug Menu success does not explain the earlier failure at base 4faf16ed.",
    "Native and consumer evidence does not establish paired performance acceptance, quiet-host qualification, or downstream RedTilde acceptance."
  ],
  "capturedAtUtc": "2026-10-25T20:04:08.6813285Z",
  "compiledMergeFirstParent": "4faf16ed647df71cd7d5623c1682738592771c3e",
  "workflowStatus": "in_progress_at_capture",
  "pairedBenchmarkStatus": "pending_at_capture"
}
```

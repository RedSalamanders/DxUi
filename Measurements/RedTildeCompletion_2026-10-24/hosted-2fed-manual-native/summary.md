# Native qualification record

This is a qualification/audit record, not a native benchmark receipt. The JSON payload below retains the reviewed values. Original JSON payload SHA-256: `48DDF9140AFA7E5103214C5BCF4ECAE5FD8A1484A032A2A106BCF7DD670B4257`.

```json
{
  "schema": "dxui-hosted-native-public-summary-v1",
  "event": "workflow_dispatch",
  "workflowRun": 40219420793,
  "reviewedHead": "2fed82465dcee9f213753754e0e8e54f353cf38f",
  "reviewedTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "mergedMainCommit": "97b26bad7fcdb0aa3dbb2efd4bceac77aab5a295",
  "mergedMainTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "compiledSourceCommit": "2fed82465dcee9f213753754e0e8e54f353cf38f",
  "compiledTree": "2c0641bc363bf713cfc709d721c3f6549dd5aef8",
  "compiledTreeMatchesReviewedAndMergedTree": true,
  "runConclusion": "success",
  "sixNativeProfilesPassed": true,
  "nativeArtifactCount": 6,
  "allProviderArchiveDigestsMatched": true,
  "artifacts": [
    {
      "profile": "ARM64-ASan-Debug",
      "archiveSha256": "2691A758D4F5E76D9257591CFCFFF7428479F96FB5220868462B529749C33CCE",
      "archiveBytes": 2947286,
      "zipEntries": 122,
      "expandedFiles": 122,
      "providerDigestMatched": true
    },
    {
      "profile": "ARM64-Debug",
      "archiveSha256": "9E7DDBA8A71C08F76D3801AD5A80A6962AB485E6922BDA6697435F132A3D6D70",
      "archiveBytes": 2937265,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    },
    {
      "profile": "ARM64-Release",
      "archiveSha256": "23E2183D6053D48BA7DD7527483BFAD928501380D6442E2D1C08D92B527B7F4C",
      "archiveBytes": 2928897,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-ASan-Debug",
      "archiveSha256": "CF3D89D7C7F26E60424933E9B7498B146654C141D1306FBC3391BE6897122BF5",
      "archiveBytes": 2949114,
      "zipEntries": 122,
      "expandedFiles": 122,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-Debug",
      "archiveSha256": "DF4530121C6352ACF85AE63B7C62AF9DE1A1C641142292CCA3306EB151A8C95B",
      "archiveBytes": 2920735,
      "zipEntries": 118,
      "expandedFiles": 118,
      "providerDigestMatched": true
    },
    {
      "profile": "x64-Release",
      "archiveSha256": "3E7857C5304E5FC7FDFBAE6F34ACC9E118B9A99FB18C706059207EC5140D730C",
      "archiveBytes": 3064726,
      "zipEntries": 134,
      "expandedFiles": 134,
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
          "reportSha256": "BFBA2DE7E61FC28C620E200315AD25FEB09C2D6C483E8C696E1576E8259B5812"
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
          "reportSha256": "CFBF3B8716C28F86240FF93659829FC431F524FD70A5528D6075FD85C8408668"
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
  "workflowStatus": "completed",
  "pairedBenchmarkStatus": "not_run_for_workflow_dispatch"
}
```

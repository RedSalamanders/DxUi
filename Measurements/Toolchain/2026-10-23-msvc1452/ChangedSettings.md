# DxUi before/after tool metadata changes

This table groups every changed captured field by tool. changed-tool-settings.csv maps each change to project/configuration/platform and full before/after values. The comparison uses each project/profile field value set; full-tool-metadata.csv.gz retains exact per-file assignments. Literal unset/default differences and dependency-output fingerprints are preserved, not replaced with assumed compiler behavior. No native source files changed.

| Tool / field | Changed project-profile rows | Configurations | Before values | After values |
| --- | ---: | --- | --- | --- |
| ClCompile / FunctionLevelLinking | 12 | Release | &lt;unset&gt; | true |
| ClCompile / WholeProgramOptimization | 12 | Release | false | true |
| Lib / LinkTimeCodeGeneration | 12 | Release | &lt;unset&gt; | true |
| Link / LinkTimeCodeGeneration | 12 | Release | &lt;unset&gt; | UseLinkTimeCodeGeneration |
| Link / OptimizeReferences | 12 | Release | &lt;unset&gt; | true |

import ghidra.app.script.GhidraScript;
import ghidra.base.project.GhidraProject;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

// Read the saved, separately verified Office reference. No import, analysis,
// transaction, save, symbol edit or current-program change is performed.
public class ReadRichEditRecalcEvidence extends GhidraScript {
    public void run() throws Exception {
        String activeBefore = currentProgram.getDomainFile().getPathname();
        Path output = Path.of("Z:/src/DxUi-worktrees/rich-text-core-d2/.build/reports/rich-ghidra-mcp-session-20261018/office-recalc");
        Files.createDirectories(output);
        GhidraProject reference = GhidraProject.openProject("Z:/src/richedit/reference/ghidra", "RichEditReference", false);
        Program program = null;
        DecompInterface decompiler = new DecompInterface();
        JsonObject evidence = new JsonObject();
        JsonArray functions = new JsonArray();
        try {
            program = reference.openProgram("/office-modern", "RICHED20.DLL", true);
            String expected = "7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41";
            if (!expected.equalsIgnoreCase(program.getExecutableSHA256()))
                throw new IllegalStateException("Saved Office reference SHA-256 mismatch");
            if (!decompiler.openProgram(program))
                throw new IllegalStateException("Cannot open saved Office reference in decompiler");
            evidence.addProperty("classification", "read-only-pinned-office-recalculation-decompilation");
            evidence.addProperty("sha256", program.getExecutableSHA256());
            evidence.addProperty("programPath", program.getDomainFile().getPathname());
            evidence.addProperty("imageBase", program.getImageBase().toString());
            evidence.addProperty("language", program.getLanguageID().toString());
            String[][] targets = {
                {"1800040b0", "CDisplayML::StepBackgroundRecalc"},
                {"180040810", "CDisplayML::WaitForRecalc"},
                {"180040990", "CDisplayEngineML::WaitForRecalc"},
                {"18003df50", "CDisplayEngineML::RecalcLines"},
                {"180041280", "CDisplayML::RecalcView"},
                {"180045170", "CDisplay::InvalidateRecalc"}
            };
            for (String[] target : targets) {
                Function function = program.getFunctionManager().getFunctionAt(
                    program.getAddressFactory().getDefaultAddressSpace().getAddress(target[0]));
                if (function == null || !function.getName(true).equals(target[1]))
                    throw new IllegalStateException("Saved function identity mismatch: " + target[1]);
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted() || result.getDecompiledFunction() == null)
                    throw new IllegalStateException("Decompilation failed: " + target[1] + ": " + result.getErrorMessage());
                String filename = target[0] + ".c";
                Files.writeString(output.resolve(filename), result.getDecompiledFunction().getC(), StandardCharsets.UTF_8);
                JsonObject item = new JsonObject();
                item.addProperty("name", function.getName(true));
                item.addProperty("entry", function.getEntryPoint().toString());
                item.addProperty("rva", "0x" + Long.toHexString(function.getEntryPoint().subtract(program.getImageBase())));
                item.addProperty("bodyBytes", function.getBody().getNumAddresses());
                item.addProperty("signature", function.getPrototypeString(true, true));
                item.addProperty("warning", result.getErrorMessage());
                item.addProperty("file", filename);
                functions.add(item);
            }
            evidence.add("functions", functions);
            evidence.addProperty("programChanged", program.isChanged());
            evidence.addProperty("activeBefore", activeBefore);
            evidence.addProperty("activeAfter", currentProgram.getDomainFile().getPathname());
            if (!activeBefore.equals(currentProgram.getDomainFile().getPathname()) || program.isChanged())
                throw new IllegalStateException("Read-only evidence changed program state");
            Files.writeString(output.resolve("inventory.receipt.txt"), new GsonBuilder().setPrettyPrinting().create().toJson(evidence), StandardCharsets.UTF_8);
            println("Verified six saved Office recalculation functions; source program unchanged; active program remains " + activeBefore);
        } finally {
            decompiler.dispose();
            if (program != null) reference.close(program);
            reference.close();
        }
    }
}

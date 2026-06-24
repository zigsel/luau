//! Emit Luau bytecode by hand via `BytecodeBuilder` (advanced; most users want
//! `luau.compile`). Opcodes are `LuauOpcode` values from `Luau/Bytecode.h`.

const std = @import("std");
const c = @import("bindings");

/// Luau VM opcodes (`enum LuauOpcode`). Non-exhaustive; the full set lives in
/// `Luau/Bytecode.h`. Pass `@intFromEnum(Op.x)` (a `u8`) to the `emit*` methods,
/// or use the `c`-prefixed raw values directly.
pub const Op = enum(u8) {
    nop = c.LOP_NOP,
    loadnil = c.LOP_LOADNIL,
    loadb = c.LOP_LOADB,
    loadn = c.LOP_LOADN,
    loadk = c.LOP_LOADK,
    move = c.LOP_MOVE,
    getglobal = c.LOP_GETGLOBAL,
    setglobal = c.LOP_SETGLOBAL,
    call = c.LOP_CALL,
    @"return" = c.LOP_RETURN,
    _,
};

/// A hand bytecode builder. Call `deinit` when done.
pub const Builder = struct {
    handle: *c.LuauBytecodeBuilder,

    pub fn init() Builder {
        return .{ .handle = c.luau_bcb_new().? };
    }
    pub fn deinit(self: Builder) void {
        c.luau_bcb_free(self.handle);
    }

    /// Begin a function frame; returns its function id.
    pub fn beginFunction(self: Builder, numparams: u8, is_vararg: bool) u32 {
        return c.luau_bcb_begin_function(self.handle, numparams, @intFromBool(is_vararg));
    }
    pub fn endFunction(self: Builder, max_stack_size: u8, num_upvalues: u8) void {
        c.luau_bcb_end_function(self.handle, max_stack_size, num_upvalues);
    }
    pub fn endFunctionFlags(self: Builder, max_stack_size: u8, num_upvalues: u8, flags: u8) void {
        c.luau_bcb_end_function_flags(self.handle, max_stack_size, num_upvalues, flags);
    }
    pub fn setMainFunction(self: Builder, fid: u32) void {
        c.luau_bcb_set_main_function(self.handle, fid);
    }

    pub fn addConstantNil(self: Builder) i32 {
        return c.luau_bcb_add_constant_nil(self.handle);
    }
    pub fn addConstantBoolean(self: Builder, value: bool) i32 {
        return c.luau_bcb_add_constant_boolean(self.handle, @intFromBool(value));
    }
    pub fn addConstantNumber(self: Builder, value: f64) i32 {
        return c.luau_bcb_add_constant_number(self.handle, value);
    }
    pub fn addConstantString(self: Builder, value: []const u8) i32 {
        return c.luau_bcb_add_constant_string(self.handle, value.ptr, value.len);
    }
    pub fn addConstantInteger(self: Builder, value: i64) i32 {
        return c.luau_bcb_add_constant_integer(self.handle, value);
    }
    pub fn addConstantVector(self: Builder, x: f32, y: f32, z: f32, w: f32) i32 {
        return c.luau_bcb_add_constant_vector(self.handle, x, y, z, w);
    }
    pub fn addImport(self: Builder, iid: u32) i32 {
        return c.luau_bcb_add_import(self.handle, iid);
    }
    pub fn addConstantClosure(self: Builder, fid: u32) i32 {
        return c.luau_bcb_add_constant_closure(self.handle, fid);
    }
    pub fn addChildFunction(self: Builder, fid: u32) i16 {
        return c.luau_bcb_add_child_function(self.handle, fid);
    }
    /// Intern a table shape constant and return its id (-1 on failure).
    /// `keys` are proto-constant indices (max 32). If `constants` is non-null it
    /// must have the same length as `keys`; use -1 for "no constant" on a key.
    pub fn addConstantTable(self: Builder, keys: []const i32, constants: ?[]const i32) i32 {
        const cptr: ?[*]const i32 = if (constants) |cc| cc.ptr else null;
        return c.luau_bcb_add_constant_table(self.handle, keys.ptr, cptr, @intCast(keys.len));
    }
    /// Add a class shape constant and return its id (-1 on failure). `class_name`
    /// and the name slices are proto-constant indices.
    pub fn addClassShape(self: Builder, class_name: i32, property_names: []const i32, method_names: []const i32) i32 {
        return c.luau_bcb_add_class_shape(
            self.handle,
            class_name,
            property_names.ptr,
            @intCast(property_names.len),
            method_names.ptr,
            @intCast(method_names.len),
        );
    }
    /// Add a feedback slot; `t` is a `LuauFeedbackType` value.
    pub fn addFbSlot(self: Builder, t: i32) u32 {
        return c.luau_bcb_add_fb_slot(self.handle, t);
    }

    pub fn emitABC(self: Builder, op: u8, a: u8, b: u8, cc: u8) void {
        c.luau_bcb_emit_abc(self.handle, op, a, b, cc);
    }
    pub fn emitAD(self: Builder, op: u8, a: u8, d: i16) void {
        c.luau_bcb_emit_ad(self.handle, op, a, d);
    }
    pub fn emitE(self: Builder, op: u8, e: i32) void {
        c.luau_bcb_emit_e(self.handle, op, e);
    }
    pub fn emitAux(self: Builder, aux: u32) void {
        c.luau_bcb_emit_aux(self.handle, aux);
    }
    pub fn undoEmit(self: Builder, op: u8) void {
        c.luau_bcb_undo_emit(self.handle, op);
    }
    /// Mark the current position as a jump label and return it.
    pub fn emitLabel(self: Builder) usize {
        return c.luau_bcb_emit_label(self.handle);
    }
    /// Patch a `D`-form jump to point at `target_label`; returns false if out of range.
    pub fn patchJumpD(self: Builder, jump_label: usize, target_label: usize) bool {
        return c.luau_bcb_patch_jump_d(self.handle, jump_label, target_label) != 0;
    }
    /// Patch a `C`-form skip; returns false if out of range.
    pub fn patchSkipC(self: Builder, jump_label: usize, target_label: usize) bool {
        return c.luau_bcb_patch_skip_c(self.handle, jump_label, target_label) != 0;
    }
    pub fn patchAux(self: Builder, target_aux: usize, new_value: i32) void {
        c.luau_bcb_patch_aux(self.handle, target_aux, new_value);
    }
    pub fn foldJumps(self: Builder) void {
        c.luau_bcb_fold_jumps(self.handle);
    }
    /// Expand long jumps; returns the rewritten instruction stream. The slice
    /// borrows the builder's storage and is valid until the next call.
    pub fn expandJumps(self: Builder) ExpandedJumps {
        return .{ .builder = self, .len = c.luau_bcb_expand_jumps_count(self.handle) };
    }

    pub fn setDebugFunctionName(self: Builder, name: []const u8) void {
        c.luau_bcb_set_debug_function_name(self.handle, name.ptr, name.len);
    }
    pub fn setDebugLine(self: Builder, line: i32) void {
        c.luau_bcb_set_debug_line(self.handle, line);
    }
    pub fn setDebugFunctionLineDefined(self: Builder, line: i32) void {
        c.luau_bcb_set_debug_function_line_defined(self.handle, line);
    }
    pub fn pushDebugLocal(self: Builder, name: []const u8, reg: u8, startpc: u32, endpc: u32) void {
        c.luau_bcb_push_debug_local(self.handle, name.ptr, name.len, reg, startpc, endpc);
    }
    pub fn pushDebugUpval(self: Builder, name: []const u8) void {
        c.luau_bcb_push_debug_upval(self.handle, name.ptr, name.len);
    }
    pub fn addDebugRemark(self: Builder, text: [:0]const u8) void {
        c.luau_bcb_add_debug_remark(self.handle, text.ptr);
    }
    pub fn needsDebugRemarks(self: Builder) bool {
        return c.luau_bcb_needs_debug_remarks(self.handle) != 0;
    }

    // --- type info ---

    pub fn setFunctionTypeInfo(self: Builder, value: []const u8) void {
        c.luau_bcb_set_function_type_info(self.handle, value.ptr, value.len);
    }
    /// `type` is a `LuauBytecodeType` value.
    pub fn pushLocalTypeInfo(self: Builder, type_: i32, reg: u8, startpc: u32, endpc: u32) void {
        c.luau_bcb_push_local_type_info(self.handle, type_, reg, startpc, endpc);
    }
    pub fn pushUpvalTypeInfo(self: Builder, type_: i32) void {
        c.luau_bcb_push_upval_type_info(self.handle, type_);
    }

    // --- userdata types ---

    pub fn addUserdataType(self: Builder, name: [:0]const u8) u32 {
        return c.luau_bcb_add_userdata_type(self.handle, name.ptr);
    }
    pub fn useUserdataType(self: Builder, index: u32) void {
        c.luau_bcb_use_userdata_type(self.handle, index);
    }

    // --- counters ---

    pub fn instructionCount(self: Builder) usize {
        return c.luau_bcb_get_instruction_count(self.handle);
    }
    pub fn totalInstructionCount(self: Builder) usize {
        return c.luau_bcb_get_total_instruction_count(self.handle);
    }
    pub fn debugPC(self: Builder) u32 {
        return c.luau_bcb_get_debug_pc(self.handle);
    }

    // --- dump / introspection ---

    /// Dump flags (`BytecodeBuilder::DumpFlags`), OR together.
    pub const DumpFlags = struct {
        pub const code: u32 = 1 << 0;
        pub const lines: u32 = 1 << 1;
        pub const source: u32 = 1 << 2;
        pub const locals: u32 = 1 << 3;
        pub const remarks: u32 = 1 << 4;
        pub const types: u32 = 1 << 5;
        pub const constants: u32 = 1 << 6;
    };

    pub fn setDumpFlags(self: Builder, flags: u32) void {
        c.luau_bcb_set_dump_flags(self.handle, flags);
    }
    pub fn setDumpSource(self: Builder, source: []const u8) void {
        c.luau_bcb_set_dump_source(self.handle, source.ptr, source.len);
    }
    /// All returned slices borrow handle-owned storage, valid until the next
    /// dump/annotate call on the same builder or until deinit.
    pub fn dumpFunction(self: Builder, id: u32) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_dump_function(self.handle, id, &len);
        return p[0..len];
    }
    pub fn dumpEverything(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_dump_everything(self.handle, &len);
        return p[0..len];
    }
    pub fn dumpSourceRemarks(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_dump_source_remarks(self.handle, &len);
        return p[0..len];
    }
    pub fn dumpTypeInfo(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_dump_type_info(self.handle, &len);
        return p[0..len];
    }
    pub fn functionData(self: Builder, id: u32) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_get_function_data(self.handle, id, &len);
        return p[0..len];
    }
    pub fn annotateInstruction(self: Builder, fid: u32, instpos: u32) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_annotate_instruction(self.handle, fid, instpos, &len);
        return p[0..len];
    }

    /// The bytecode string table; entries borrow handle storage.
    pub fn stringTable(self: Builder) StringTable {
        return .{ .builder = self, .len = c.luau_bcb_string_table_count(self.handle) };
    }

    pub fn finalize(self: Builder) void {
        c.luau_bcb_finalize(self.handle);
    }
    /// The finalized bytecode bytes (valid until the next finalize/deinit).
    pub fn bytecode(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcb_get_bytecode(self.handle, &len);
        return p[0..len];
    }
};

/// Result of `expandJumps`; index into the rewritten instruction stream.
pub const ExpandedJumps = struct {
    builder: Builder,
    len: usize,

    pub fn at(self: ExpandedJumps, i: usize) u32 {
        return c.luau_bcb_expand_jumps_at(self.builder.handle, i);
    }
};

/// The bytecode string table; entries borrow handle storage until the next call.
pub const StringTable = struct {
    builder: Builder,
    len: usize,

    pub fn at(self: StringTable, i: usize) []const u8 {
        var l: usize = 0;
        const p = c.luau_bcb_string_table_at(self.builder.handle, i, &l);
        return p[0..l];
    }
};

// --- static helpers (no builder instance required) ---

pub fn getImportId1(id0: i32) u32 {
    return c.luau_bcb_get_import_id_1(id0);
}
pub fn getImportId2(id0: i32, id1: i32) u32 {
    return c.luau_bcb_get_import_id_2(id0, id1);
}
pub fn getImportId3(id0: i32, id1: i32, id2: i32) u32 {
    return c.luau_bcb_get_import_id_3(id0, id1, id2);
}
/// Decompose an encoded import id; returns the number of valid components (1..3).
pub fn decomposeImportId(ids: u32, out: *[3]i32) i32 {
    return c.luau_bcb_decompose_import_id(ids, &out[0], &out[1], &out[2]);
}
pub fn getStringHash(key: []const u8) u32 {
    return c.luau_bcb_get_string_hash(key.ptr, key.len);
}
/// Wrap a message as a bytecode error. Free the result with `freeError`.
pub fn getError(message: [:0]const u8) [*:0]u8 {
    return c.luau_bcb_get_error(message.ptr);
}
pub fn freeError(s: [*:0]u8) void {
    c.luau_bcb_free_string(s);
}
pub fn getVersion() u8 {
    return c.luau_bcb_get_version();
}
pub fn getTypeEncodingVersion() u8 {
    return c.luau_bcb_get_type_encoding_version();
}

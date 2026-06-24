//! Idiomatic wrapper over the Luau CodeGen IR (`IrBuilder` / `IrData`, via the
//! C++ shim) — the JIT intermediate-representation construction + inspection
//! kit.
//!
//! VM-internal boundary: the high-level lowering entry points
//! (`buildFunctionIr` / `translateInst` / ...) take a VM-internal `Proto*` and
//! are NOT bound here — see `shim/codegen/ir.cpp`. What IS exposed is the
//! Proto-free factory surface (constants, operands, instructions, blocks) plus
//! read-only inspection of the resulting IR function.

const std = @import("std");
const c = @import("bindings");

/// Operand kind, mirrors `IrOpKind`.
pub const OpKind = enum(u8) {
    none = c.LUAU_IR_OP_NONE,
    undef = c.LUAU_IR_OP_UNDEF,
    constant = c.LUAU_IR_OP_CONSTANT,
    condition = c.LUAU_IR_OP_CONDITION,
    inst = c.LUAU_IR_OP_INST,
    block = c.LUAU_IR_OP_BLOCK,
    vmreg = c.LUAU_IR_OP_VMREG,
    vmconst = c.LUAU_IR_OP_VMCONST,
    vmupvalue = c.LUAU_IR_OP_VMUPVALUE,
    vmexit = c.LUAU_IR_OP_VMEXIT,
    _,
};

/// Constant kind, mirrors `IrConstKind`.
pub const ConstKind = enum(u8) {
    int = c.LUAU_IR_CONST_INT,
    int64 = c.LUAU_IR_CONST_INT64,
    uint = c.LUAU_IR_CONST_UINT,
    double = c.LUAU_IR_CONST_DOUBLE,
    tag = c.LUAU_IR_CONST_TAG,
    import = c.LUAU_IR_CONST_IMPORT,
    _,
};

/// IR comparison condition, mirrors `IrCondition`.
pub const Condition = enum(u8) {
    equal = c.LUAU_IR_COND_EQUAL,
    not_equal = c.LUAU_IR_COND_NOT_EQUAL,
    less = c.LUAU_IR_COND_LESS,
    not_less = c.LUAU_IR_COND_NOT_LESS,
    less_equal = c.LUAU_IR_COND_LESS_EQUAL,
    not_less_equal = c.LUAU_IR_COND_NOT_LESS_EQUAL,
    greater = c.LUAU_IR_COND_GREATER,
    not_greater = c.LUAU_IR_COND_NOT_GREATER,
    greater_equal = c.LUAU_IR_COND_GREATER_EQUAL,
    not_greater_equal = c.LUAU_IR_COND_NOT_GREATER_EQUAL,
    unsigned_less = c.LUAU_IR_COND_UNSIGNED_LESS,
    unsigned_less_equal = c.LUAU_IR_COND_UNSIGNED_LESS_EQUAL,
    unsigned_greater = c.LUAU_IR_COND_UNSIGNED_GREATER,
    unsigned_greater_equal = c.LUAU_IR_COND_UNSIGNED_GREATER_EQUAL,
};

/// Basic-block kind, mirrors `IrBlockKind`.
pub const BlockKind = enum(u8) {
    bytecode = c.LUAU_IR_BLOCK_BYTECODE,
    fallback = c.LUAU_IR_BLOCK_FALLBACK,
    internal = c.LUAU_IR_BLOCK_INTERNAL,
    linearized = c.LUAU_IR_BLOCK_LINEARIZED,
    exitsync = c.LUAU_IR_BLOCK_EXITSYNC,
    dead = c.LUAU_IR_BLOCK_DEAD,
};

/// A packed IR operand reference (kind in low 4 bits, index in high 28). Pass
/// `.none` for unused instruction operand slots.
pub const Op = struct {
    raw: c.LuauIrOp,

    pub const none: Op = .{ .raw = 0 };

    pub fn kind(self: Op) OpKind {
        return @enumFromInt(@as(u8, @intCast(c.luau_irop_kind(self.raw) & 0xff)));
    }
    pub fn index(self: Op) u32 {
        return c.luau_irop_index(self.raw);
    }
};

fn wrap(raw: c.LuauIrOp) Op {
    return .{ .raw = raw };
}

/// An IR builder + the function it populates. Owns its storage; call `deinit`.
pub const Builder = struct {
    handle: *c.LuauIrBuilder,

    pub fn init() error{OutOfMemory}!Builder {
        const h = c.luau_irbuilder_new() orelse return error.OutOfMemory;
        return .{ .handle = h };
    }

    pub fn deinit(self: Builder) void {
        c.luau_irbuilder_free(self.handle);
    }

    // ---- constant / operand factories -----------------------------------

    pub fn undef(self: Builder) Op {
        return wrap(c.luau_irbuilder_undef(self.handle));
    }
    pub fn constInt(self: Builder, value: i32) Op {
        return wrap(c.luau_irbuilder_const_int(self.handle, value));
    }
    pub fn constInt64(self: Builder, value: i64) Op {
        return wrap(c.luau_irbuilder_const_int64(self.handle, value));
    }
    pub fn constUint(self: Builder, value: u32) Op {
        return wrap(c.luau_irbuilder_const_uint(self.handle, value));
    }
    pub fn constDouble(self: Builder, value: f64) Op {
        return wrap(c.luau_irbuilder_const_double(self.handle, value));
    }
    pub fn constTag(self: Builder, value: u8) Op {
        return wrap(c.luau_irbuilder_const_tag(self.handle, value));
    }
    pub fn constImport(self: Builder, value: u32) Op {
        return wrap(c.luau_irbuilder_const_import(self.handle, value));
    }
    pub fn cond(self: Builder, condition: Condition) Op {
        return wrap(c.luau_irbuilder_cond(self.handle, @intFromEnum(condition)));
    }

    pub fn vmReg(self: Builder, idx: u8) Op {
        return wrap(c.luau_irbuilder_vmreg(self.handle, idx));
    }
    pub fn vmConst(self: Builder, idx: u32) Op {
        return wrap(c.luau_irbuilder_vmconst(self.handle, idx));
    }
    pub fn vmUpvalue(self: Builder, idx: u8) Op {
        return wrap(c.luau_irbuilder_vmupvalue(self.handle, idx));
    }
    pub fn vmExit(self: Builder, pcpos: u32) Op {
        return wrap(c.luau_irbuilder_vmexit(self.handle, pcpos));
    }

    /// A constant of `k` whose union storage holds the raw 64-bit `bits`
    /// (e.g. `@bitCast(f64)` for a double). `as_common_key` participates in
    /// constant de-duplication. Mirrors `IrBuilder::constAny`.
    pub fn constAny(self: Builder, k: ConstKind, bits: u64, as_common_key: u64) Op {
        return wrap(c.luau_irbuilder_const_any(self.handle, @intFromEnum(k), bits, as_common_key));
    }

    pub fn block(self: Builder, k: BlockKind) Op {
        return wrap(c.luau_irbuilder_block(self.handle, @intFromEnum(k)));
    }
    /// A block whose instruction range begins at `index`.
    pub fn blockAtInst(self: Builder, index: u32) Op {
        return wrap(c.luau_irbuilder_block_at_inst(self.handle, index));
    }
    /// A fallback block for bytecode pc position `pcpos`.
    pub fn fallbackBlock(self: Builder, pcpos: u32) Op {
        return wrap(c.luau_irbuilder_fallback_block(self.handle, pcpos));
    }
    pub fn beginBlock(self: Builder, b: Op) void {
        c.luau_irbuilder_begin_block(self.handle, b.raw);
    }
    /// True if `b` (a Block-kind operand) refers to an Internal-kind block.
    pub fn isInternalBlock(self: Builder, b: Op) bool {
        return c.luau_irbuilder_is_internal_block(self.handle, b.raw) != 0;
    }

    /// Emit a guarded tag load+check: load the tag at `loc` and branch to
    /// `fallback` if it is not `tag`.
    pub fn loadAndCheckTag(self: Builder, loc: Op, tag: u8, fallback: Op) void {
        c.luau_irbuilder_load_and_check_tag(self.handle, loc.raw, tag, fallback.raw);
    }
    /// Emit a "safe environment" check for bytecode pc position `pcpos`.
    pub fn checkSafeEnv(self: Builder, pcpos: i32) void {
        c.luau_irbuilder_check_safe_env(self.handle, pcpos);
    }

    /// Emit an instruction. `cmd` is the raw `IrCmd` enum value (that enum is
    /// large and volatile, so pass the integer directly). Provide 0–7 operands;
    /// longer slices route through the variadic shim entry point.
    pub fn inst(self: Builder, cmd: u8, ops: []const Op) Op {
        return switch (ops.len) {
            0 => wrap(c.luau_irbuilder_inst0(self.handle, cmd)),
            1 => wrap(c.luau_irbuilder_inst1(self.handle, cmd, ops[0].raw)),
            2 => wrap(c.luau_irbuilder_inst2(self.handle, cmd, ops[0].raw, ops[1].raw)),
            3 => wrap(c.luau_irbuilder_inst3(self.handle, cmd, ops[0].raw, ops[1].raw, ops[2].raw)),
            4 => wrap(c.luau_irbuilder_inst4(self.handle, cmd, ops[0].raw, ops[1].raw, ops[2].raw, ops[3].raw)),
            5 => wrap(c.luau_irbuilder_inst5(self.handle, cmd, ops[0].raw, ops[1].raw, ops[2].raw, ops[3].raw, ops[4].raw)),
            6 => wrap(c.luau_irbuilder_inst6(self.handle, cmd, ops[0].raw, ops[1].raw, ops[2].raw, ops[3].raw, ops[4].raw, ops[5].raw)),
            7 => wrap(c.luau_irbuilder_inst7(self.handle, cmd, ops[0].raw, ops[1].raw, ops[2].raw, ops[3].raw, ops[4].raw, ops[5].raw, ops[6].raw)),
            else => wrap(c.luau_irbuilder_inst_n(self.handle, cmd, @ptrCast(ops.ptr), ops.len)),
        };
    }

    // ---- inspection ------------------------------------------------------

    pub fn constCount(self: Builder) usize {
        return c.luau_irbuilder_const_count(self.handle);
    }
    pub fn instCount(self: Builder) usize {
        return c.luau_irbuilder_inst_count(self.handle);
    }
    pub fn blockCount(self: Builder) usize {
        return c.luau_irbuilder_block_count(self.handle);
    }

    /// Kind of the constant at `idx`, or null if out of range.
    pub fn constKind(self: Builder, idx: usize) ?ConstKind {
        const k = c.luau_irbuilder_const_kind(self.handle, idx);
        return if (k < 0) null else @enumFromInt(@as(u8, @intCast(k)));
    }
    pub fn constIntAt(self: Builder, idx: usize) i32 {
        return c.luau_irbuilder_const_get_int(self.handle, idx);
    }
    pub fn constInt64At(self: Builder, idx: usize) i64 {
        return c.luau_irbuilder_const_get_int64(self.handle, idx);
    }
    pub fn constUintAt(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_const_get_uint(self.handle, idx);
    }
    pub fn constDoubleAt(self: Builder, idx: usize) f64 {
        return c.luau_irbuilder_const_get_double(self.handle, idx);
    }
    pub fn constTagAt(self: Builder, idx: usize) u8 {
        return c.luau_irbuilder_const_get_tag(self.handle, idx);
    }
    /// Raw 64-bit union storage of the constant at `idx` (e.g. the IEEE-754
    /// bits of a double; `@bitCast` to recover the float).
    pub fn constBitsAt(self: Builder, idx: usize) u64 {
        return c.luau_irbuilder_const_get_bits(self.handle, idx);
    }

    /// Raw `IrCmd` value of the instruction at `idx`, or null if out of range.
    pub fn instCmd(self: Builder, idx: usize) ?u8 {
        const v = c.luau_irbuilder_inst_cmd(self.handle, idx);
        return if (v < 0) null else @intCast(v);
    }
    pub fn instOpCount(self: Builder, idx: usize) usize {
        return c.luau_irbuilder_inst_op_count(self.handle, idx);
    }
    pub fn instOp(self: Builder, idx: usize, op_idx: usize) Op {
        return wrap(c.luau_irbuilder_inst_op(self.handle, idx, op_idx));
    }
    /// Last instruction index that uses the value at `idx` (liveness).
    pub fn instLastUse(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_inst_last_use(self.handle, idx);
    }
    /// Number of recorded uses of the value at `idx`.
    pub fn instUseCount(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_inst_use_count(self.handle, idx);
    }

    // ---- block inspection (per-block) ------------------------------------

    /// Kind of the block at `idx`, or null if out of range.
    pub fn blockKind(self: Builder, idx: usize) ?BlockKind {
        const k = c.luau_irbuilder_block_kind(self.handle, idx);
        return if (k < 0) null else @enumFromInt(@as(u8, @intCast(k)));
    }
    pub fn blockFlags(self: Builder, idx: usize) u8 {
        return c.luau_irbuilder_block_flags(self.handle, idx);
    }
    pub fn blockUseCount(self: Builder, idx: usize) u16 {
        return c.luau_irbuilder_block_use_count(self.handle, idx);
    }
    /// Inclusive start of the block's instruction-index range (~0 if unset).
    pub fn blockStart(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_block_start(self.handle, idx);
    }
    /// Inclusive finish of the block's instruction-index range (~0 if unset).
    pub fn blockFinish(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_block_finish(self.handle, idx);
    }
    /// Bytecode pc position at which the block was generated (~0 if none).
    pub fn blockStartpc(self: Builder, idx: usize) u32 {
        return c.luau_irbuilder_block_startpc(self.handle, idx);
    }
    /// Number of instructions belonging to block `idx` (0 if its range is
    /// unset). Combine with `blockInstAt` to walk the block's instructions.
    pub fn blockInstCount(self: Builder, idx: usize) usize {
        return c.luau_irbuilder_block_inst_count(self.handle, idx);
    }
    /// The `n`-th instruction index that belongs to block `idx`, or null if out
    /// of range. Feed into the `inst*` getters.
    pub fn blockInstAt(self: Builder, idx: usize, n: usize) ?usize {
        const v = c.luau_irbuilder_block_inst_at(self.handle, idx, n);
        return if (v == ~@as(u32, 0)) null else v;
    }

    // ---- function / builder state ----------------------------------------

    /// Entry block index of the built function.
    pub fn entryBlock(self: Builder) u32 {
        return c.luau_irbuilder_entry_block(self.handle);
    }
    /// Whether the active block has already been terminated.
    pub fn inTerminatedBlock(self: Builder) bool {
        return c.luau_irbuilder_in_terminated_block(self.handle) != 0;
    }
    /// Index of the block currently being emitted into, or null if none.
    pub fn activeBlockIdx(self: Builder) ?u32 {
        const v = c.luau_irbuilder_active_block_idx(self.handle);
        return if (v == ~@as(u32, 0)) null else v;
    }
};

//! The Luau bytecode graph (`Luau::Bytecode`, `BytecodeGraph.h`): lift a
//! function's bytecode into blocks/instructions/constants for read-only
//! inspection and round-trip serialization.

const std = @import("std");
const c = @import("bindings");

/// A `BcOp` reference: a tagged index into one of the graph's collections.
pub const BcOp = struct {
    pub const Kind = enum(c_int) {
        none = c.LUAU_BCOP_NONE,
        imm = c.LUAU_BCOP_IMM,
        inst = c.LUAU_BCOP_INST,
        block = c.LUAU_BCOP_BLOCK,
        phi = c.LUAU_BCOP_PHI,
        proj = c.LUAU_BCOP_PROJ,
        vm_reg = c.LUAU_BCOP_VMREG,
        vm_const = c.LUAU_BCOP_VMCONST,
        vm_upvalue = c.LUAU_BCOP_VMUPVALUE,
        vm_proto = c.LUAU_BCOP_VMPROTO,
        _,
    };
    kind: Kind,
    index: u32,
};

/// Kind of a constant (`BcVmConstKind`).
pub const ConstKind = enum(c_int) {
    nil = c.LUAU_BCVMCONST_NIL,
    boolean = c.LUAU_BCVMCONST_BOOLEAN,
    number = c.LUAU_BCVMCONST_NUMBER,
    vector = c.LUAU_BCVMCONST_VECTOR,
    string = c.LUAU_BCVMCONST_STRING,
    import = c.LUAU_BCVMCONST_IMPORT,
    table = c.LUAU_BCVMCONST_TABLE,
    closure = c.LUAU_BCVMCONST_CLOSURE,
    integer = c.LUAU_BCVMCONST_INTEGER,
    _,
};

/// Kind of an immediate (`BcImmKind`).
pub const ImmKind = enum(c_int) {
    boolean = c.LUAU_BCIMM_BOOLEAN,
    int = c.LUAU_BCIMM_INT,
    import = c.LUAU_BCIMM_IMPORT,
    _,
};

/// Kind of a control-flow edge (`BcBlockEdgeKind`).
pub const EdgeKind = enum(c_int) {
    branch = c.LUAU_BCEDGE_BRANCH,
    fallthrough = c.LUAU_BCEDGE_FALLTHROUGH,
    loop = c.LUAU_BCEDGE_LOOP,
    _,
};

/// A single function's bytecode lifted into a graph of blocks/instructions/
/// constants. Read-only inspection plus round-trip serialization. Call `deinit`.
///
/// String views in the graph (constant strings, debug names) borrow the string
/// table you passed to `fromBytecode`; it must outlive the graph.
pub const Graph = struct {
    handle: *c.LuauBcGraph,

    /// Parse a function bytecode blob. `strings` is the string table backing
    /// string constants/names; it (and the bytes it points to) must outlive the
    /// returned graph. `allocator` is used only transiently to build the parallel
    /// pointer/length arrays the C API expects and is freed before returning.
    /// Returns null if the blob could not be parsed or on allocation failure.
    pub fn fromBytecode(allocator: std.mem.Allocator, bytecode: []const u8, strings: []const []const u8) ?Graph {
        const ptrs = allocator.alloc([*c]const u8, strings.len) catch return null;
        defer allocator.free(ptrs);
        const lens = allocator.alloc(usize, strings.len) catch return null;
        defer allocator.free(lens);
        for (strings, 0..) |s, i| {
            ptrs[i] = s.ptr;
            lens[i] = s.len;
        }
        const h = c.luau_bcg_from_function_bytecode(
            bytecode.ptr,
            bytecode.len,
            ptrs.ptr,
            lens.ptr,
            strings.len,
        ) orelse return null;
        return .{ .handle = h };
    }

    pub fn deinit(self: Graph) void {
        c.luau_bcg_free(self.handle);
    }

    /// Serialize the graph back to a function bytecode blob (valid until the next
    /// call or `deinit`). Returns null on error.
    pub fn toBytecode(self: Graph) ?[]const u8 {
        var len: usize = 0;
        const p = c.luau_bcg_to_function_bytecode(self.handle, &len) orelse return null;
        return p[0..len];
    }

    pub fn maxStackSize(self: Graph) u8 {
        return c.luau_bcg_max_stack_size(self.handle);
    }
    pub fn numParams(self: Graph) u8 {
        return c.luau_bcg_num_params(self.handle);
    }
    pub fn numUpvalues(self: Graph) u8 {
        return c.luau_bcg_num_upvalues(self.handle);
    }
    pub fn isVararg(self: Graph) bool {
        return c.luau_bcg_is_vararg(self.handle) != 0;
    }
    pub fn flags(self: Graph) u8 {
        return c.luau_bcg_flags(self.handle);
    }
    pub fn lineDefined(self: Graph) u32 {
        return c.luau_bcg_line_defined(self.handle);
    }
    pub fn debugName(self: Graph) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcg_debug_name(self.handle, &len);
        return p[0..len];
    }
    pub fn typeInfo(self: Graph) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcg_type_info(self.handle, &len);
        return p[0..len];
    }
    /// Entry/exit block index, or null if unset.
    pub fn entryBlock(self: Graph) ?u32 {
        const v = c.luau_bcg_entry_block(self.handle);
        return if (v < 0) null else @intCast(v);
    }
    pub fn exitBlock(self: Graph) ?u32 {
        const v = c.luau_bcg_exit_block(self.handle);
        return if (v < 0) null else @intCast(v);
    }

    pub fn blockCount(self: Graph) usize {
        return c.luau_bcg_block_count(self.handle);
    }
    pub fn instCount(self: Graph) usize {
        return c.luau_bcg_inst_count(self.handle);
    }
    pub fn constCount(self: Graph) usize {
        return c.luau_bcg_const_count(self.handle);
    }
    pub fn immCount(self: Graph) usize {
        return c.luau_bcg_imm_count(self.handle);
    }
    pub fn phiCount(self: Graph) usize {
        return c.luau_bcg_phi_count(self.handle);
    }
    pub fn projCount(self: Graph) usize {
        return c.luau_bcg_proj_count(self.handle);
    }
    pub fn protoCount(self: Graph) usize {
        return c.luau_bcg_proto_count(self.handle);
    }
    pub fn protoAt(self: Graph, i: usize) u32 {
        return c.luau_bcg_proto_at(self.handle, i);
    }
    pub fn upvalueTypeCount(self: Graph) usize {
        return c.luau_bcg_upvalue_type_count(self.handle);
    }
    /// `LuauBytecodeType` value.
    pub fn upvalueTypeAt(self: Graph, i: usize) i32 {
        return c.luau_bcg_upvalue_type_at(self.handle, i);
    }
    pub fn upvalueNameCount(self: Graph) usize {
        return c.luau_bcg_upvalue_name_count(self.handle);
    }
    pub fn upvalueNameAt(self: Graph, i: usize) []const u8 {
        var len: usize = 0;
        const p = c.luau_bcg_upvalue_name_at(self.handle, i, &len);
        return p[0..len];
    }

    pub const LocalType = struct { type_: i32, reg: u8, startpc: u32, endpc: u32 };
    pub fn localTypeCount(self: Graph) usize {
        return c.luau_bcg_local_type_count(self.handle);
    }
    pub fn localTypeAt(self: Graph, i: usize) LocalType {
        return .{
            .type_ = c.luau_bcg_local_type_at(self.handle, i),
            .reg = c.luau_bcg_local_type_reg_at(self.handle, i),
            .startpc = c.luau_bcg_local_type_startpc_at(self.handle, i),
            .endpc = c.luau_bcg_local_type_endpc_at(self.handle, i),
        };
    }

    pub const DebugLocal = struct { name: []const u8, reg: u8, startpc: u32, endpc: u32 };
    pub fn debugLocalCount(self: Graph) usize {
        return c.luau_bcg_debug_local_count(self.handle);
    }
    pub fn debugLocalAt(self: Graph, i: usize) DebugLocal {
        var len: usize = 0;
        const p = c.luau_bcg_debug_local_name_at(self.handle, i, &len);
        return .{
            .name = p[0..len],
            .reg = c.luau_bcg_debug_local_reg_at(self.handle, i),
            .startpc = c.luau_bcg_debug_local_startpc_at(self.handle, i),
            .endpc = c.luau_bcg_debug_local_endpc_at(self.handle, i),
        };
    }

    // --- instructions ---
    /// Instruction opcode (`LuauOpcode`).
    pub fn instOp(self: Graph, i: usize) i32 {
        return c.luau_bcg_inst_op(self.handle, i);
    }
    pub fn instLine(self: Graph, i: usize) u32 {
        return c.luau_bcg_inst_line(self.handle, i);
    }
    pub fn instLastUse(self: Graph, i: usize) u32 {
        return c.luau_bcg_inst_last_use(self.handle, i);
    }
    pub fn instUseCount(self: Graph, i: usize) u32 {
        return c.luau_bcg_inst_use_count(self.handle, i);
    }
    pub fn instBlock(self: Graph, i: usize) ?u32 {
        const v = c.luau_bcg_inst_block(self.handle, i);
        return if (v < 0) null else @intCast(v);
    }
    pub fn instOperandCount(self: Graph, i: usize) usize {
        return c.luau_bcg_inst_operand_count(self.handle, i);
    }
    pub fn instOperandAt(self: Graph, i: usize, k: usize) BcOp {
        return .{
            .kind = @enumFromInt(c.luau_bcg_inst_operand_kind(self.handle, i, k)),
            .index = c.luau_bcg_inst_operand_index(self.handle, i, k),
        };
    }

    // --- blocks ---
    pub fn blockFlags(self: Graph, i: usize) u8 {
        return c.luau_bcg_block_flags(self.handle, i);
    }
    pub fn blockUseCount(self: Graph, i: usize) u32 {
        return c.luau_bcg_block_use_count(self.handle, i);
    }
    pub fn blockStartpc(self: Graph, i: usize) u32 {
        return c.luau_bcg_block_startpc(self.handle, i);
    }
    pub fn blockOpCount(self: Graph, i: usize) usize {
        return c.luau_bcg_block_op_count(self.handle, i);
    }
    pub fn blockOpAt(self: Graph, i: usize, k: usize) BcOp {
        return .{
            .kind = @enumFromInt(c.luau_bcg_block_op_kind(self.handle, i, k)),
            .index = c.luau_bcg_block_op_index(self.handle, i, k),
        };
    }

    pub const Edge = struct { kind: EdgeKind, target: ?u32 };
    pub fn blockSuccessorCount(self: Graph, i: usize) usize {
        return c.luau_bcg_block_successor_count(self.handle, i);
    }
    pub fn blockPredecessorCount(self: Graph, i: usize) usize {
        return c.luau_bcg_block_predecessor_count(self.handle, i);
    }
    pub fn blockSuccessorAt(self: Graph, i: usize, e: usize) Edge {
        const t = c.luau_bcg_block_successor_target(self.handle, i, e);
        return .{
            .kind = @enumFromInt(c.luau_bcg_block_successor_kind(self.handle, i, e)),
            .target = if (t < 0) null else @intCast(t),
        };
    }
    pub fn blockPredecessorAt(self: Graph, i: usize, e: usize) Edge {
        const t = c.luau_bcg_block_predecessor_target(self.handle, i, e);
        return .{
            .kind = @enumFromInt(c.luau_bcg_block_predecessor_kind(self.handle, i, e)),
            .target = if (t < 0) null else @intCast(t),
        };
    }

    // --- immediates ---
    pub const Imm = union(enum) { boolean: bool, int: i32, import: u32 };
    pub fn immAt(self: Graph, i: usize) Imm {
        const kind: ImmKind = @enumFromInt(c.luau_bcg_imm_kind(self.handle, i));
        return switch (kind) {
            .boolean => .{ .boolean = c.luau_bcg_imm_boolean(self.handle, i) != 0 },
            .int => .{ .int = c.luau_bcg_imm_int(self.handle, i) },
            .import => .{ .import = c.luau_bcg_imm_import(self.handle, i) },
            else => .{ .int = c.luau_bcg_imm_int(self.handle, i) },
        };
    }

    // --- constants ---
    pub const Const = union(enum) {
        nil,
        boolean: bool,
        number: f64,
        vector: [4]f32,
        string: []const u8,
        import: u32,
        table: u32,
        closure: u32,
        integer: i64,
        other: ConstKind,
    };
    pub fn constAt(self: Graph, i: usize) Const {
        const kind: ConstKind = @enumFromInt(c.luau_bcg_const_kind(self.handle, i));
        return switch (kind) {
            .nil => .nil,
            .boolean => .{ .boolean = c.luau_bcg_const_boolean(self.handle, i) != 0 },
            .number => .{ .number = c.luau_bcg_const_number(self.handle, i) },
            .vector => .{ .vector = .{
                c.luau_bcg_const_vector(self.handle, i, 0),
                c.luau_bcg_const_vector(self.handle, i, 1),
                c.luau_bcg_const_vector(self.handle, i, 2),
                c.luau_bcg_const_vector(self.handle, i, 3),
            } },
            .string => blk: {
                var len: usize = 0;
                const p = c.luau_bcg_const_string(self.handle, i, &len);
                break :blk .{ .string = p[0..len] };
            },
            .import => .{ .import = c.luau_bcg_const_import(self.handle, i) },
            .table => .{ .table = c.luau_bcg_const_table(self.handle, i) },
            .closure => .{ .closure = c.luau_bcg_const_closure(self.handle, i) },
            .integer => .{ .integer = c.luau_bcg_const_integer(self.handle, i) },
            else => .{ .other = kind },
        };
    }

    // --- phi / projection ---
    pub fn phiOperandCount(self: Graph, i: usize) usize {
        return c.luau_bcg_phi_operand_count(self.handle, i);
    }
    pub fn phiOperandAt(self: Graph, i: usize, k: usize) BcOp {
        return .{
            .kind = @enumFromInt(c.luau_bcg_phi_operand_kind(self.handle, i, k)),
            .index = c.luau_bcg_phi_operand_index(self.handle, i, k),
        };
    }
    pub const Proj = struct { op: BcOp, index: u32 };
    pub fn projAt(self: Graph, i: usize) Proj {
        return .{
            .op = .{
                .kind = @enumFromInt(c.luau_bcg_proj_op_kind(self.handle, i)),
                .index = c.luau_bcg_proj_op_index(self.handle, i),
            },
            .index = c.luau_bcg_proj_index(self.handle, i),
        };
    }
};

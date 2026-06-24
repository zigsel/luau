//! Idiomatic enums mirroring the Luau C enums and tag spaces.

const c = @import("bindings");

/// A Luau value type, mirroring `enum lua_Type` plus the `LUA_TNONE` sentinel.
/// Non-exhaustive so future VM tags never make `@enumFromInt` illegal.
pub const LuaType = enum(c_int) {
    none = c.LUA_TNONE, // -1
    nil = c.LUA_TNIL, // 0
    boolean = c.LUA_TBOOLEAN, // 1
    lightuserdata = c.LUA_TLIGHTUSERDATA, // 2
    number = c.LUA_TNUMBER, // 3
    integer = c.LUA_TINTEGER, // 4
    vector = c.LUA_TVECTOR, // 5
    string = c.LUA_TSTRING, // 6
    table = c.LUA_TTABLE, // 7
    function = c.LUA_TFUNCTION, // 8
    userdata = c.LUA_TUSERDATA, // 9
    thread = c.LUA_TTHREAD, // 10
    buffer = c.LUA_TBUFFER, // 11
    class = c.LUA_TCLASS, // 12
    object = c.LUA_TOBJECT, // 13
    _,

    pub fn fromInt(v: c_int) LuaType {
        return @enumFromInt(v);
    }
    pub fn toInt(t: LuaType) c_int {
        return @intFromEnum(t);
    }
};

/// Garbage-collector control op, mirroring `enum lua_GCOp`, used by `lua_gc`.
pub const GcAction = enum(c_int) {
    stop = c.LUA_GCSTOP,
    restart = c.LUA_GCRESTART,
    collect = c.LUA_GCCOLLECT,
    count = c.LUA_GCCOUNT,
    countb = c.LUA_GCCOUNTB,
    is_running = c.LUA_GCISRUNNING,
    step = c.LUA_GCSTEP,
    set_goal = c.LUA_GCSETGOAL,
    set_step_mul = c.LUA_GCSETSTEPMUL,
    set_step_size = c.LUA_GCSETSTEPSIZE,
    _,
};

/// Well-known pseudo-indices.
pub const registry_index: c_int = c.LUA_REGISTRYINDEX;
pub const environ_index: c_int = c.LUA_ENVIRONINDEX;
pub const globals_index: c_int = c.LUA_GLOBALSINDEX;

/// "All results" sentinel for `call`/`pcall`.
pub const multret: c_int = c.LUA_MULTRET;

/// Reference sentinels.
pub const noref: c_int = c.LUA_NOREF;
pub const refnil: c_int = c.LUA_REFNIL;

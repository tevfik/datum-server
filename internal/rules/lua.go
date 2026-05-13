package rules

import (
	"context"
	"fmt"
	"time"

	lua "github.com/yuin/gopher-lua"
)

const (
	// luaMaxExecTime is the maximum execution time for a single Lua script.
	luaMaxExecTime = 100 * time.Millisecond
)

// LuaEvaluator provides a sandboxed Lua execution environment for rule evaluation.
// Dangerous modules (os, io, debug, loadfile, dofile) are removed.
// Each evaluation runs in an isolated, time-limited context.
type LuaEvaluator struct{}

// NewLuaEvaluator creates a new Lua evaluator.
func NewLuaEvaluator() *LuaEvaluator {
	return &LuaEvaluator{}
}

// Evaluate runs a Lua script against device data and returns whether the rule matched.
// The script receives a global `ctx` table with:
//   - ctx.device_id (string)
//   - ctx.data (table with telemetry fields)
//
// The script MUST return true or false.
//
// Example script:
//
//	local temp = ctx.data.temperature
//	local hum = ctx.data.humidity
//	return temp > 30 and hum < 40
func (le *LuaEvaluator) Evaluate(script string, deviceID string, data map[string]interface{}) (bool, error) {
	L := lua.NewState(lua.Options{
		SkipOpenLibs: true,
	})
	defer L.Close()

	// Open only safe libraries
	openSafeLibs(L)

	// Set up timeout context
	ctx, cancel := context.WithTimeout(context.Background(), luaMaxExecTime)
	defer cancel()
	L.SetContext(ctx)

	// Build the ctx table
	ctxTable := L.NewTable()

	// ctx.device_id
	L.SetField(ctxTable, "device_id", lua.LString(deviceID))

	// ctx.data
	dataTable := mapToLuaTable(L, data)
	L.SetField(ctxTable, "data", dataTable)

	// ctx.time (current unix timestamp)
	L.SetField(ctxTable, "time", lua.LNumber(float64(time.Now().Unix())))

	// Register ctx as global
	L.SetGlobal("ctx", ctxTable)

	// Execute the script
	if err := L.DoString(script); err != nil {
		return false, fmt.Errorf("lua execution error: %w", err)
	}

	// Get the return value
	ret := L.Get(-1)
	L.Pop(1)

	switch v := ret.(type) {
	case lua.LBool:
		return bool(v), nil
	case *lua.LNilType:
		return false, nil
	case lua.LNumber:
		return float64(v) != 0, nil
	default:
		return false, fmt.Errorf("lua script must return boolean, got %s", ret.Type().String())
	}
}

// openSafeLibs opens only safe standard libraries, excluding os, io, debug, etc.
func openSafeLibs(L *lua.LState) {
	// Base library (print, type, tostring, tonumber, etc.) — but remove dangerous functions
	lua.OpenBase(L)
	lua.OpenTable(L)
	lua.OpenString(L)
	lua.OpenMath(L)

	// Remove dangerous base functions
	for _, name := range []string{
		"dofile", "loadfile", "load", "loadstring",
		"collectgarbage", "rawset", "rawget", "rawequal", "rawlen",
		"require", "module",
	} {
		L.SetGlobal(name, lua.LNil)
	}
}

// mapToLuaTable converts a Go map to a Lua table recursively.
func mapToLuaTable(L *lua.LState, m map[string]interface{}) *lua.LTable {
	t := L.NewTable()
	for k, v := range m {
		t.RawSetString(k, goToLuaValue(L, v))
	}
	return t
}

// goToLuaValue converts a Go value to a Lua value.
func goToLuaValue(L *lua.LState, v interface{}) lua.LValue {
	if v == nil {
		return lua.LNil
	}
	switch val := v.(type) {
	case float64:
		return lua.LNumber(val)
	case float32:
		return lua.LNumber(float64(val))
	case int:
		return lua.LNumber(float64(val))
	case int64:
		return lua.LNumber(float64(val))
	case string:
		return lua.LString(val)
	case bool:
		return lua.LBool(val)
	case map[string]interface{}:
		return mapToLuaTable(L, val)
	case []interface{}:
		t := L.NewTable()
		for i, item := range val {
			t.RawSetInt(i+1, goToLuaValue(L, item))
		}
		return t
	default:
		return lua.LString(fmt.Sprintf("%v", val))
	}
}

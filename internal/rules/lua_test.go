package rules

import (
	"testing"
)

func TestLuaEvaluator_BasicTrue(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `return ctx.data.temperature > 30`
	data := map[string]interface{}{"temperature": 35.0}

	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !result {
		t.Fatal("expected true, got false")
	}
}

func TestLuaEvaluator_BasicFalse(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `return ctx.data.temperature > 30`
	data := map[string]interface{}{"temperature": 25.0}

	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if result {
		t.Fatal("expected false, got true")
	}
}

func TestLuaEvaluator_ComplexLogic(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `
		local temp = ctx.data.temperature
		local hum = ctx.data.humidity
		-- Heat index approximation
		local hi = temp + (0.5 * (hum - 40))
		return hi > 35
	`
	data := map[string]interface{}{
		"temperature": 30.0,
		"humidity":    80.0,
	}

	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !result {
		t.Fatal("expected true for high heat index, got false")
	}
}

func TestLuaEvaluator_AccessDeviceID(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `return ctx.device_id == "sensor_01"`
	data := map[string]interface{}{"temp": 20.0}

	result, err := eval.Evaluate(script, "sensor_01", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !result {
		t.Fatal("expected true for matching device_id")
	}
}

func TestLuaEvaluator_StringContains(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `return string.find(ctx.data.status, "error") ~= nil`
	data := map[string]interface{}{"status": "sensor_error_overflow"}

	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !result {
		t.Fatal("expected true for string containing 'error'")
	}
}

func TestLuaEvaluator_SandboxBlocked(t *testing.T) {
	eval := NewLuaEvaluator()

	// dofile should be nil in sandbox
	script := `
		if dofile then
			return true
		end
		return false
	`
	data := map[string]interface{}{}
	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if result {
		t.Fatal("dofile should not be available in sandbox")
	}
}

func TestLuaEvaluator_SyntaxError(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `this is not valid lua!!!`
	data := map[string]interface{}{}

	_, err := eval.Evaluate(script, "dev_1", data)
	if err == nil {
		t.Fatal("expected error for invalid lua syntax")
	}
}

func TestLuaEvaluator_MathOperations(t *testing.T) {
	eval := NewLuaEvaluator()
	script := `
		local avg = (ctx.data.t1 + ctx.data.t2 + ctx.data.t3) / 3
		return avg > 25
	`
	data := map[string]interface{}{
		"t1": 28.0,
		"t2": 26.0,
		"t3": 30.0,
	}

	result, err := eval.Evaluate(script, "dev_1", data)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !result {
		t.Fatal("expected true for average > 25")
	}
}

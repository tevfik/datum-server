package quota

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestDefaultPlanFree(t *testing.T) {
	m := New()
	p := m.PlanFor("anyone")
	assert.Equal(t, "free", p.Name)
}

func TestSetUserPlan(t *testing.T) {
	m := New()
	m.SetUserPlan("u1", "pro")
	assert.Equal(t, "pro", m.PlanFor("u1").Name)
	m.SetUserPlan("u1", "unknown")
	assert.Equal(t, "free", m.PlanFor("u1").Name) // fallback
}

func TestCheckBeforeIncrement(t *testing.T) {
	m := New()
	require.NoError(t, m.Check("u1", ResourceMCPCall, 1))
}

func TestIncrementAndExceed(t *testing.T) {
	m := New()
	// Free plan: device hard cap = 10.
	for i := 0; i < 10; i++ {
		require.NoError(t, m.Check("u1", ResourceDevice, 1))
		m.Increment("u1", ResourceDevice, 1)
	}
	err := m.Check("u1", ResourceDevice, 1)
	require.Error(t, err)
	var qe *Error
	require.True(t, errors.As(err, &qe))
	assert.Equal(t, ResourceDevice, qe.Resource)
	assert.EqualValues(t, 10, qe.Used)
	assert.EqualValues(t, 10, qe.Limit)
}

func TestDecrementFreesQuota(t *testing.T) {
	m := New()
	for i := 0; i < 10; i++ {
		m.Increment("u1", ResourceDevice, 1)
	}
	m.Decrement("u1", ResourceDevice, 1)
	require.NoError(t, m.Check("u1", ResourceDevice, 1))
}

func TestUnlimitedPlan(t *testing.T) {
	m := New()
	m.SetUserPlan("u1", "unlimited")
	for i := 0; i < 50; i++ {
		require.NoError(t, m.Check("u1", ResourceDevice, 1))
		m.Increment("u1", ResourceDevice, 1)
	}
}

func TestUsageReport(t *testing.T) {
	m := New()
	m.Increment("u1", ResourceMCPCall, 5)
	u := m.Usage("u1")
	assert.EqualValues(t, 5, u[ResourceMCPCall][PeriodHour])
	assert.EqualValues(t, 5, u[ResourceMCPCall][PeriodDay])
	assert.EqualValues(t, 5, u[ResourceMCPCall][PeriodTotal])
}

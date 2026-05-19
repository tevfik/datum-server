package rules

import (
	"sync"

	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/robfig/cron/v3"
)

// Scheduler manages cron-based rule triggers.
// For rules with trigger.type = "scheduled", it creates cron jobs that
// periodically fetch the latest device data and evaluate the rule.
type Scheduler struct {
	mu     sync.Mutex
	cron   *cron.Cron
	engine *Engine
	store  storage.Provider
	jobs   map[string]cron.EntryID // rule_id -> cron entry ID
}

// NewScheduler creates a new rule scheduler.
func NewScheduler(engine *Engine, store storage.Provider) *Scheduler {
	return &Scheduler{
		cron:   cron.New(cron.WithSeconds()),
		engine: engine,
		store:  store,
		jobs:   make(map[string]cron.EntryID),
	}
}

// Start begins the cron scheduler and registers all scheduled rules.
func (s *Scheduler) Start() {
	log := logger.GetLogger()

	// Register all existing scheduled rules
	rules := s.engine.GetScheduledRules()
	for _, r := range rules {
		if err := s.registerRule(r); err != nil {
			log.Warn().
				Err(err).
				Str("rule_id", r.ID).
				Str("schedule", r.Trigger.Schedule).
				Msg("Failed to register scheduled rule")
		}
	}

	s.cron.Start()
	log.Info().
		Int("scheduled_rules", len(rules)).
		Msg("Rule scheduler started")
}

// Stop gracefully shuts down the scheduler.
func (s *Scheduler) Stop() {
	ctx := s.cron.Stop()
	<-ctx.Done()
}



// registerRule creates a cron job for a scheduled rule (must hold mu).
func (s *Scheduler) registerRule(r *Rule) error {
	if r.Trigger.Schedule == "" {
		return nil
	}

	log := logger.GetLogger()
	ruleID := r.ID

	entryID, err := s.cron.AddFunc(r.Trigger.Schedule, func() {
		s.evaluateScheduledRule(ruleID)
	})
	if err != nil {
		return err
	}

	s.jobs[ruleID] = entryID
	log.Debug().
		Str("rule_id", ruleID).
		Str("schedule", r.Trigger.Schedule).
		Msg("Registered scheduled rule")

	return nil
}

// evaluateScheduledRule fetches the latest data for a rule's device and evaluates it.
func (s *Scheduler) evaluateScheduledRule(ruleID string) {
	log := logger.GetLogger()

	r, ok := s.engine.GetRule(ruleID)
	if !ok || !r.Enabled {
		return
	}

	deviceID := r.effectiveDeviceID()
	if deviceID == "" {
		log.Warn().
			Str("rule_id", ruleID).
			Msg("Scheduled rule has no device_id, skipping")
		return
	}

	// Fetch the latest data point for this device
	latest, err := s.store.GetLatestData(deviceID)
	if err != nil || latest == nil {
		log.Debug().
			Str("rule_id", ruleID).
			Str("device_id", deviceID).
			Msg("No data available for scheduled rule evaluation")
		return
	}

	// Evaluate the rule against the latest data
	s.engine.EvaluateManual(ruleID, deviceID, latest.Data)
}

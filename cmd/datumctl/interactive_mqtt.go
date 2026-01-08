package main

import (
	"fmt"

	"github.com/AlecAivazis/survey/v2"
)

// mqttMenu handles the MQTT Management submenu
func mqttMenu() error {
	for {
		var action string
		prompt := &survey.Select{
			Message: "MQTT Management:",
			Options: []string{
				"List Clients",
				"Broker Stats",
				"Publish Message",
				"← Back to main menu",
			},
		}

		if err := survey.AskOne(prompt, &action); err != nil {
			return err
		}

		switch action {
		case "List Clients":
			fmt.Println("\n> datumctl mqtt clients")
			return runMQTTClients(nil, nil)

		case "Broker Stats":
			fmt.Println("\n> datumctl mqtt stats")
			return runMQTTStats(nil, nil)

		case "Publish Message":
			return promptMQTTPublish()

		case "← Back to main menu":
			return nil
		}
	}
}

func promptMQTTPublish() error {
	var topic string
	if err := survey.AskOne(&survey.Input{
		Message: "Topic:",
	}, &topic, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var message string
	if err := survey.AskOne(&survey.Input{
		Message: "Message:",
	}, &message, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var retain bool
	if err := survey.AskOne(&survey.Confirm{
		Message: "Retain?",
		Default: false,
	}, &retain); err != nil {
		return err
	}

	mqttTopic = topic
	mqttMessage = message
	mqttRetain = retain

	retainFlag := ""
	if retain {
		retainFlag = " --retain"
	}

	fmt.Printf("\n> datumctl mqtt publish --topic %q --message %q%s\n", topic, message, retainFlag)
	return runMQTTPublish(nil, nil)
}

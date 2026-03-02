package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"gopkg.in/yaml.v3"
)

type ComposeFile struct {
	Version  string                    `yaml:"version" json:"version"`
	Services map[string]ComposeService `yaml:"services" json:"services"`
	Networks map[string]interface{}    `yaml:"networks" json:"networks"`
	Volumes  map[string]interface{}    `yaml:"volumes" json:"volumes"`
}

type ComposeService struct {
	Image         string      `yaml:"image" json:"image"`
	Build         interface{} `yaml:"build" json:"build"` // string context or struct
	Ports         []string    `yaml:"ports" json:"ports"`
	Environment   interface{} `yaml:"environment" json:"environment"` // map or array
	EnvFile       interface{} `yaml:"env_file" json:"env_file"`       // string or array
	Volumes       []string    `yaml:"volumes" json:"volumes"`
	Networks      []string    `yaml:"networks" json:"networks"`
	DependsOn     []string    `yaml:"depends_on" json:"depends_on"`
	ContainerName string      `yaml:"container_name" json:"container_name"`
	Command       interface{} `yaml:"command" json:"command"`       // string or array
	Entrypoint    interface{} `yaml:"entrypoint" json:"entrypoint"` // string or array
	WorkingDir    string      `yaml:"working_dir" json:"working_dir"`
	CPUs          string      `yaml:"cpus" json:"cpus"`
	MemLimit      string      `yaml:"mem_limit" json:"mem_limit"`
	DNS           interface{} `yaml:"dns" json:"dns"` // string or array
	ReadOnly      bool        `yaml:"read_only" json:"read_only"`
	Init          bool        `yaml:"init" json:"init"`
}

type BuildInfo struct {
	Context    string            `yaml:"context" json:"context"`
	Dockerfile string            `yaml:"dockerfile" json:"dockerfile"`
	Args       map[string]string `yaml:"args" json:"args"`
}

type CliContainer struct {
	Configuration struct {
		ID     string            `json:"id"`
		Labels map[string]string `json:"labels"`
		Image  struct {
			Reference string `json:"reference"`
		} `json:"image"`
	} `json:"configuration"`
	Status      string  `json:"status"`
	StartedDate float64 `json:"startedDate"`
}

func parseComposeFile(path string) (*ComposeFile, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	var cf ComposeFile
	if err := yaml.Unmarshal(data, &cf); err != nil {
		return nil, err
	}

	return &cf, nil
}

func (cf *ComposeFile) getSortedServices() ([]string, error) {
	visited := make(map[string]bool)
	temp := make(map[string]bool)
	var result []string

	var visit func(string) error
	visit = func(name string) error {
		if temp[name] {
			return fmt.Errorf("cycle detected in depends_on for service %s", name)
		}
		if visited[name] {
			return nil
		}
		temp[name] = true
		service := cf.Services[name]
		for _, dep := range service.DependsOn {
			if _, ok := cf.Services[dep]; !ok {
				log.Printf("[Compose] Warning: service %s depends on non-existent service %s", name, dep)
				continue
			}
			if err := visit(dep); err != nil {
				return err
			}
		}
		temp[name] = false
		visited[name] = true
		result = append(result, name)
		return nil
	}

	for name := range cf.Services {
		if !visited[name] {
			if err := visit(name); err != nil {
				return nil, err
			}
		}
	}
	return result, nil
}

func (cf *ComposeFile) composeUp(projectDir string, projectName string) error {
	// 1. Create networks
	// Always create a default network for the project if not explicitly defined with others
	defaultNet := fmt.Sprintf("%s-default", projectName)
	log.Printf("[Compose] Ensuring default network: %s", defaultNet)
	exec.Command("container", "network", "create", defaultNet).Run()

	for netName := range cf.Networks {
		fullNetName := fmt.Sprintf("%s-%s", projectName, netName)
		log.Printf("[Compose] Creating network: %s", fullNetName)
		exec.Command("container", "network", "create", fullNetName).Run()
	}

	// 2. Create volumes
	for volName := range cf.Volumes {
		fullVolName := fmt.Sprintf("%s-%s", projectName, volName)
		log.Printf("[Compose] Creating volume: %s", fullVolName)
		exec.Command("container", "volume", "create", fullVolName).Run()
	}

	// 3. Sort services
	sorted, err := cf.getSortedServices()
	if err != nil {
		return err
	}

	// 4. Launch in order
	for _, name := range sorted {
		if err := cf.launchService(name, cf.Services[name], projectDir, projectName); err != nil {
			return err
		}
	}

	return nil
}

func (cf *ComposeFile) launchService(name string, service ComposeService, projectDir string, projectName string) error {
	imageName := service.Image
	if imageName == "" {
		imageName = fmt.Sprintf("%s-%s", projectName, name)
	}

	// Handle build if specified
	if service.Build != nil {
		context := "."
		dockerfile := "Dockerfile"

		switch b := service.Build.(type) {
		case string:
			context = b
		case map[string]interface{}:
			if c, ok := b["context"].(string); ok {
				context = c
			}
			if d, ok := b["dockerfile"].(string); ok {
				dockerfile = d
			}
		case BuildInfo: // If we use BuildInfo struct
			context = b.Context
			dockerfile = b.Dockerfile
		}

		absContext := context
		if !filepath.IsAbs(context) {
			absContext = filepath.Join(projectDir, context)
		}

		absDockerfile := dockerfile
		if !filepath.IsAbs(dockerfile) {
			absDockerfile = filepath.Join(absContext, dockerfile)
		}

		log.Printf("[Compose] Building image for %s: %s (Context: %s, Dockerfile: %s)", name, imageName, absContext, absDockerfile)
		// We use CombinedOutput to capture errors for better reporting
		cmd := exec.Command("container", "build", "-t", imageName, "-f", absDockerfile, absContext)
		cmd.Dir = absContext
		if out, err := cmd.CombinedOutput(); err != nil {
			return fmt.Errorf("failed to build image %s: %s: %w", imageName, string(out), err)
		}
	}

	// Construct run arguments
	args := []string{"run", "-d"}

	// Labels for tracking
	args = append(args, "-l", fmt.Sprintf("cider.compose.project=%s", projectName))
	args = append(args, "-l", fmt.Sprintf("cider.compose.service=%s", name))

	// Container name and Hostname
	containerName := service.ContainerName
	if containerName == "" {
		containerName = fmt.Sprintf("%s-%s", projectName, name)
	}
	args = append(args, "--name", containerName)

	// DNS Discovery
	args = append(args, "--dns-search", projectName)
	args = append(args, "--dns-domain", projectName)

	// Ports
	for _, p := range service.Ports {
		args = append(args, "-p", p)
	}

	// Environment
	if service.Environment != nil {
		switch env := service.Environment.(type) {
		case map[string]interface{}:
			for k, v := range env {
				args = append(args, "-e", fmt.Sprintf("%s=%v", k, v))
			}
		case []interface{}:
			for _, v := range env {
				args = append(args, "-e", fmt.Sprintf("%v", v))
			}
		}
	}

	// Env File
	if service.EnvFile != nil {
		switch ef := service.EnvFile.(type) {
		case string:
			args = append(args, "--env-file", filepath.Join(projectDir, ef))
		case []interface{}:
			for _, v := range ef {
				args = append(args, "--env-file", filepath.Join(projectDir, fmt.Sprintf("%v", v)))
			}
		}
	}

	// Volumes
	for _, v := range service.Volumes {
		parts := strings.Split(v, ":")
		if len(parts) >= 2 {
			hostPath := parts[0]
			if !filepath.IsAbs(hostPath) {
				hostPath = filepath.Join(projectDir, hostPath)
			}
			args = append(args, "-v", fmt.Sprintf("%s:%s", hostPath, parts[1]))
		}
	}

	// Networks
	if len(service.Networks) > 0 {
		for _, net := range service.Networks {
			fullNetName := fmt.Sprintf("%s-%s", projectName, net)
			args = append(args, "--network", fullNetName)
		}
	} else {
		// Use project default network
		args = append(args, "--network", fmt.Sprintf("%s-default", projectName))
	}

	// Resources
	if service.CPUs != "" {
		args = append(args, "-c", service.CPUs)
	}
	if service.MemLimit != "" {
		args = append(args, "-m", service.MemLimit)
	}

	// DNS
	if service.DNS != nil {
		switch d := service.DNS.(type) {
		case string:
			args = append(args, "--dns", d)
		case []interface{}:
			for _, v := range d {
				args = append(args, "--dns", fmt.Sprintf("%v", v))
			}
		}
	}

	// Flags
	if service.ReadOnly {
		args = append(args, "--read-only")
	}
	if service.Init {
		args = append(args, "--init")
	}

	// Image and Command
	args = append(args, imageName)

	if service.Command != nil {
		switch cmd := service.Command.(type) {
		case string:
			args = append(args, strings.Fields(cmd)...)
		case []interface{}:
			for _, v := range cmd {
				args = append(args, fmt.Sprintf("%v", v))
			}
		}
	}

	log.Printf("[Compose] Starting service %s: %s", name, strings.Join(args, " "))
	cmd := exec.Command("container", args...)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("failed to start service %s: %s: %w", name, string(out), err)
	}

	return nil
}

func composeDown(projectName string) error {
	// Find all containers with project label
	out, err := exec.Command("container", "list", "--all", "--format", "json").Output()
	if err != nil {
		return err
	}

	var containers []CliContainer
	if err := json.Unmarshal(out, &containers); err != nil {
		return err
	}

	for _, c := range containers {
		if c.Configuration.Labels["cider.compose.project"] == projectName {
			log.Printf("[Compose] Stopping and removing container: %s", c.Configuration.ID)
			exec.Command("container", "stop", c.Configuration.ID).Run()
			exec.Command("container", "delete", c.Configuration.ID).Run()
		}
	}

	// Delete networks
	// Note: We don't have a direct list of created nets in Down without the YAML,
	// but we can prune or use the naming convention
	// For now, let's at least try to remove the default one
	defaultNet := fmt.Sprintf("%s-default", projectName)
	log.Printf("[Compose] Removing default network: %s", defaultNet)
	exec.Command("container", "network", "delete", defaultNet).Run()

	return nil
}

func composeStatus(projectName string) ([]CliContainer, error) {
	out, err := exec.Command("container", "list", "--all", "--format", "json").Output()
	if err != nil {
		return nil, err
	}

	var all []CliContainer
	if err := json.Unmarshal(out, &all); err != nil {
		return nil, err
	}

	var project []CliContainer
	for _, c := range all {
		if c.Configuration.Labels["cider.compose.project"] == projectName {
			project = append(project, c)
		}
	}
	return project, nil
}

package main

import (
	"context"
	_ "embed"
	"encoding/json"
	"fmt"
	"log"
	"strings"

	"github.com/modelcontextprotocol/go-sdk/mcp"
)

//go:embed extension_api.json
var embeddedExtensionAPI []byte

// ExtensionAPI structure matches the Godot extension_api.json
type ExtensionAPI struct {
	Header         map[string]interface{} `json:"header"`
	GlobalEnums    []GlobalEnum           `json:"global_enums"`
	BuiltinClasses []BuiltinClass         `json:"builtin_classes"`
	Classes        []ClassDetail          `json:"classes"`
	Singletons     []Singleton            `json:"singletons"`
}

type GlobalEnum struct {
	Name   string      `json:"name"`
	Values []EnumValue `json:"values"`
}

type EnumValue struct {
	Name  string `json:"name"`
	Value int    `json:"value"`
}

type BuiltinClass struct {
	Name      string         `json:"name"`
	Members   []Property     `json:"members"`
	Methods   []MethodDetail `json:"methods"`
	Constants []Constant     `json:"constants"`
}

type Constant struct {
	Name  string      `json:"name"`
	Type  string      `json:"type"`
	Value interface{} `json:"value"`
}

type ClassDetail struct {
	Name       string         `json:"name"`
	Inherits   string         `json:"inherits"`
	Methods    []MethodDetail `json:"methods"`
	Properties []Property      `json:"properties"`
	Enums      []GlobalEnum   `json:"enums"`
}

type MethodDetail struct {
	Name       string     `json:"name"`
	ReturnType string     `json:"return_type"`
	Arguments  []Argument `json:"arguments"`
}

type Argument struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

type Property struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

type Singleton struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

var apiData ExtensionAPI

func loadAPI() error {
	return json.Unmarshal(embeddedExtensionAPI, &apiData)
}

// Input structs for tools
type SearchInput struct {
	Query string `json:"query" jsonschema:"the string to search for in class or method names"`
}

type ClassInput struct {
	ClassName string `json:"className" jsonschema:"the name of the Godot class to look up"`
}

type EmptyInput struct{}

// Output struct for tools
type StringOutput struct {
	Content string `json:"content" jsonschema:"the tool output"`
}

// Tool implementation: SearchAPI
func SearchAPI(ctx context.Context, req *mcp.CallToolRequest, input SearchInput) (*mcp.CallToolResult, StringOutput, error) {
	var results []string
	query := strings.ToLower(input.Query)

	// Search regular classes
	for _, class := range apiData.Classes {
		if strings.Contains(strings.ToLower(class.Name), query) {
			results = append(results, fmt.Sprintf("[Class] %s (inherits %s)", class.Name, class.Inherits))
		}
		for _, method := range class.Methods {
			if strings.Contains(strings.ToLower(method.Name), query) {
				results = append(results, fmt.Sprintf("[Method] %s::%s", class.Name, method.Name))
			}
		}
	}

	// Search builtin classes
	for _, builtin := range apiData.BuiltinClasses {
		if strings.Contains(strings.ToLower(builtin.Name), query) {
			results = append(results, fmt.Sprintf("[Builtin] %s", builtin.Name))
		}
		for _, method := range builtin.Methods {
			if strings.Contains(strings.ToLower(method.Name), query) {
				results = append(results, fmt.Sprintf("[Builtin Method] %s::%s", builtin.Name, method.Name))
			}
		}
	}

	resText := strings.Join(results, "\n")
	if len(results) == 0 {
		resText = "No matches found."
	} else if len(results) > 50 {
		resText = strings.Join(results[:50], "\n") + "\n... and more results"
	}

	return nil, StringOutput{Content: resText}, nil
}

// Tool implementation: GetClassDetails
func GetClassDetails(ctx context.Context, req *mcp.CallToolRequest, input ClassInput) (*mcp.CallToolResult, StringOutput, error) {
	for _, class := range apiData.Classes {
		if class.Name == input.ClassName {
			var sb strings.Builder
			sb.WriteString(fmt.Sprintf("Class: %s\nInherits: %s\n\nMethods:\n", class.Name, class.Inherits))
			for _, m := range class.Methods {
				var args []string
				for _, a := range m.Arguments {
					args = append(args, fmt.Sprintf("%s: %s", a.Name, a.Type))
				}
				sb.WriteString(fmt.Sprintf("- %s(%s) -> %s\n", m.Name, strings.Join(args, ", "), m.ReturnType))
			}
			if len(class.Properties) > 0 {
				sb.WriteString("\nProperties:\n")
				for _, p := range class.Properties {
					sb.WriteString(fmt.Sprintf("- %s: %s\n", p.Name, p.Type))
				}
			}
			if len(class.Enums) > 0 {
				sb.WriteString("\nEnums:\n")
				for _, e := range class.Enums {
					sb.WriteString(fmt.Sprintf("- %s\n", e.Name))
				}
			}
			return nil, StringOutput{Content: sb.String()}, nil
		}
	}
	return nil, StringOutput{Content: "Class not found: " + input.ClassName}, nil
}

// New Tools

func ListClasses(ctx context.Context, req *mcp.CallToolRequest, _ EmptyInput) (*mcp.CallToolResult, StringOutput, error) {
	var names []string
	for _, c := range apiData.Classes {
		names = append(names, c.Name)
	}
	return nil, StringOutput{Content: "Available Classes:\n" + strings.Join(names, ", ")}, nil
}

func GetSingletons(ctx context.Context, req *mcp.CallToolRequest, _ EmptyInput) (*mcp.CallToolResult, StringOutput, error) {
	var results []string
	for _, s := range apiData.Singletons {
		results = append(results, fmt.Sprintf("- %s (Type: %s)", s.Name, s.Type))
	}
	return nil, StringOutput{Content: "Engine Singletons:\n" + strings.Join(results, "\n")}, nil
}

func GetGlobalEnums(ctx context.Context, req *mcp.CallToolRequest, _ EmptyInput) (*mcp.CallToolResult, StringOutput, error) {
	var sb strings.Builder
	for _, ge := range apiData.GlobalEnums {
		sb.WriteString(fmt.Sprintf("Enum: %s\n", ge.Name))
		for _, v := range ge.Values {
			sb.WriteString(fmt.Sprintf("  %s = %d\n", v.Name, v.Value))
		}
		sb.WriteString("\n")
	}
	return nil, StringOutput{Content: sb.String()}, nil
}

func GetBuiltinClass(ctx context.Context, req *mcp.CallToolRequest, input ClassInput) (*mcp.CallToolResult, StringOutput, error) {
	for _, b := range apiData.BuiltinClasses {
		if b.Name == input.ClassName {
			var sb strings.Builder
			sb.WriteString(fmt.Sprintf("Builtin Class: %s\n", b.Name))
			if len(b.Members) > 0 {
				sb.WriteString("\nMembers:\n")
				for _, m := range b.Members {
					sb.WriteString(fmt.Sprintf("- %s: %s\n", m.Name, m.Type))
				}
			}
			sb.WriteString("\nMethods:\n")
			for _, m := range b.Methods {
				var args []string
				for _, a := range m.Arguments {
					args = append(args, fmt.Sprintf("%s: %s", a.Name, a.Type))
				}
				sb.WriteString(fmt.Sprintf("- %s(%s) -> %s\n", m.Name, strings.Join(args, ", "), m.ReturnType))
			}
			return nil, StringOutput{Content: sb.String()}, nil
		}
	}
	return nil, StringOutput{Content: "Builtin class not found: " + input.ClassName}, nil
}

func main() {
	if err := loadAPI(); err != nil {
		log.Fatalf("Failed to load embedded API: %v", err)
	}

	s := mcp.NewServer(&mcp.Implementation{
		Name:    "godot-cpp-mcp",
		Version: "1.1.0",
	}, nil)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "search_api",
		Description: "Search for classes or methods in the Godot API",
	}, SearchAPI)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "get_class_details",
		Description: "Get detailed information about a Godot class",
	}, GetClassDetails)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "list_classes",
		Description: "List all available GDExtension classes",
	}, ListClasses)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "get_singletons",
		Description: "List all Godot engine singletons",
	}, GetSingletons)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "get_global_enums",
		Description: "Get all global enums and their constant values",
	}, GetGlobalEnums)

	mcp.AddTool(s, &mcp.Tool{
		Name:        "get_builtin_class",
		Description: "Get details for builtin types like Vector3, Color, or Variant",
	}, GetBuiltinClass)

	if err := s.Run(context.Background(), &mcp.StdioTransport{}); err != nil {
		log.Fatal(err)
	}
}

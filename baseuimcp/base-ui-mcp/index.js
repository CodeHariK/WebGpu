import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
    CallToolRequestSchema,
    ListToolsRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const DOCS_DIR = path.resolve(__dirname, '../base-ui/docs/src/app/(docs)/react/components');

async function getAvailableComponents() {
    try {
        const entries = await fs.readdir(DOCS_DIR, { withFileTypes: true });
        return entries
            .filter((entry) => entry.isDirectory() && !entry.name.startsWith('.'))
            .map((entry) => entry.name);
    } catch (error) {
        if (error.code === 'ENOENT') {
            return [];
        }
        throw error;
    }
}

const server = new Server(
    {
        name: 'base-ui-mcp',
        version: '1.0.0',
    },
    {
        capabilities: {
            tools: {},
        },
    }
);

server.setRequestHandler(ListToolsRequestSchema, async () => {
    return {
        tools: [
            {
                name: 'base_ui_list_components',
                description: 'Lists all available Base UI components with documentation.',
                inputSchema: {
                    type: 'object',
                    properties: {},
                },
            },
            {
                name: 'base_ui_get_component_doc',
                description: 'Retrieves the Base UI documentation for a specific component.',
                inputSchema: {
                    type: 'object',
                    properties: {
                        componentName: {
                            type: 'string',
                            description: 'The slug/name of the component (e.g., button, accordion).',
                        },
                    },
                    required: ['componentName'],
                },
            },
        ],
    };
});

server.setRequestHandler(CallToolRequestSchema, async (request) => {
    switch (request.params.name) {
        case 'base_ui_list_components': {
            const components = await getAvailableComponents();
            return {
                content: [
                    {
                        type: 'text',
                        text: components.length > 0
                            ? `Available Base UI components:\n${components.join('\n')}`
                            : 'No components found. Ensure the base-ui documentation is present at ' + DOCS_DIR,
                    },
                ],
            };
        }

        case 'base_ui_get_component_doc': {
            const componentName = String(request.params.arguments?.componentName || '');
            if (!componentName) {
                throw new Error('componentName is required');
            }

            const docPath = path.join(DOCS_DIR, componentName, 'page.mdx');
            try {
                const content = await fs.readFile(docPath, 'utf8');
                return {
                    content: [
                        {
                            type: 'text',
                            text: content,
                        },
                    ],
                };
            } catch (error) {
                if (error.code === 'ENOENT') {
                    return {
                        content: [
                            {
                                type: 'text',
                                text: `Documentation not found for component '${componentName}'. Use base_ui_list_components to see available components.`,
                            },
                        ],
                        isError: true,
                    };
                }
                throw error;
            }
        }

        default:
            throw new Error(`Unknown tool: ${request.params.name}`);
    }
});

async function run() {
    const transport = new StdioServerTransport();
    await server.connect(transport);
    console.error('Base UI MCP server running on stdio');
}

run().catch((error) => {
    console.error('Fatal error in Base UI MCP server:', error);
    process.exit(1);
});

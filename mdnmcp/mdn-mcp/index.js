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
const DOCS_DIR = path.resolve(__dirname, '../content/files/en-us');

let docPathsCache = null;

async function walkDir(dir, baseDir) {
    let results = [];
    try {
        const list = await fs.readdir(dir, { withFileTypes: true });
        for (const entry of list) {
            const fullPath = path.join(dir, entry.name);
            if (entry.isDirectory()) {
                results = results.concat(await walkDir(fullPath, baseDir));
            } else if (entry.name === 'index.md') {
                results.push(path.relative(baseDir, dir).replace(/\\/g, '/'));
            }
        }
    } catch (err) {
        if (err.code !== 'ENOENT') throw err;
    }
    return results;
}

async function getAvailableDocs() {
    if (!docPathsCache) {
        console.error('Indexing MDN Web Docs paths from:', DOCS_DIR);
        docPathsCache = await walkDir(DOCS_DIR, DOCS_DIR);
        console.error(`Index complete. Found ${docPathsCache.length} documents.`);
    }
    return docPathsCache;
}

const server = new Server(
    {
        name: 'mdn-mcp',
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
                name: 'mdn_search_docs',
                description: 'Searches MDN Web Docs by matching keywords against document URLs (e.g. "Array map", "flex-direction"). Returns matching documentation paths that can be retrieved with mdn_get_doc.',
                inputSchema: {
                    type: 'object',
                    properties: {
                        query: {
                            type: 'string',
                            description: 'Keywords to search for in the MDN documentation paths. E.g., "array map" or "css flex".',
                        },
                    },
                    required: ['query'],
                },
            },
            {
                name: 'mdn_get_doc',
                description: 'Retrieves the Markdown content of an MDN Web Docs article.',
                inputSchema: {
                    type: 'object',
                    properties: {
                        docPath: {
                            type: 'string',
                            description: 'The path of the document to retrieve, as returned by mdn_search_docs (e.g., "web/javascript/reference/global_objects/array/map").',
                        },
                    },
                    required: ['docPath'],
                },
            },
        ],
    };
});

server.setRequestHandler(CallToolRequestSchema, async (request) => {
    switch (request.params.name) {
        case 'mdn_search_docs': {
            const query = String(request.params.arguments?.query || '').toLowerCase().trim();
            const docs = await getAvailableDocs();

            const keywords = query.split(/\s+/);
            const results = docs.filter(docPath => {
                const lowerPath = docPath.toLowerCase();
                return keywords.every(kw => lowerPath.includes(kw));
            });

            return {
                content: [
                    {
                        type: 'text',
                        text: results.length > 0
                            ? `Found ${results.length} matching documents (showing up to 50):\n${results.slice(0, 50).join('\n')}`
                            : `No documents found matching "${query}".`,
                    },
                ],
            };
        }

        case 'mdn_get_doc': {
            const docPath = String(request.params.arguments?.docPath || '');
            if (!docPath) {
                throw new Error('docPath is required');
            }

            // Secure path joining
            const safePath = path.normalize(docPath).replace(/^(\.\.(\/|\\|$))+/, '');
            const fullPath = path.join(DOCS_DIR, safePath, 'index.md');

            try {
                const content = await fs.readFile(fullPath, 'utf8');
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
                                text: `Documentation not found at path '${docPath}'. Use mdn_search_docs to find valid paths.`,
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
    console.error('MDN MCP server running on stdio');
}

run().catch((error) => {
    console.error('Fatal error in MDN MCP server:', error);
    process.exit(1);
});

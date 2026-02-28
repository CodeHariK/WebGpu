import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as cheerio from 'cheerio';
import { convert } from 'html-to-text';

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
    CallToolRequestSchema,
    ListToolsRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const WEBGPU_DOC_PATH = path.resolve(__dirname, '../WebGPU.html');
const WGSL_DOC_PATH = path.resolve(__dirname, '../WebGPU Shading Language.html');

let webgpuDoc = null;
let wgslDoc = null;

async function getParsedDoc(type) {
    if (type === 'wgsl') {
        if (!wgslDoc) {
            console.error('Parsing WGSL HTML...');
            const html = await fs.readFile(WGSL_DOC_PATH, 'utf-8');
            wgslDoc = cheerio.load(html);
            console.error('WGSL HTML Parsed');
        }
        return wgslDoc;
    } else {
        if (!webgpuDoc) {
            console.error('Parsing WebGPU HTML...');
            const html = await fs.readFile(WEBGPU_DOC_PATH, 'utf-8');
            webgpuDoc = cheerio.load(html);
            console.error('WebGPU HTML Parsed');
        }
        return webgpuDoc;
    }
}

const server = new Server(
    {
        name: 'webgpu-mcp',
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
                name: 'webgpu_list_sections',
                description: 'Searches the WebGPU or WGSL HTML document for headers and returns their IDs and text. Use this to find the section ID to read.',
                inputSchema: {
                    type: 'object',
                    properties: {
                        docType: {
                            type: 'string',
                            enum: ['webgpu', 'wgsl'],
                            description: 'Which document to search in.',
                        },
                        query: {
                            type: 'string',
                            description: 'Optional keyword to filter the header text or IDs. If omitted, returns all headers, which could be very large.',
                        },
                    },
                    required: ['docType'],
                },
            },
            {
                name: 'webgpu_read_section',
                description: 'Reads the text content of a specific section by its header ID.',
                inputSchema: {
                    type: 'object',
                    properties: {
                        docType: {
                            type: 'string',
                            enum: ['webgpu', 'wgsl'],
                        },
                        sectionId: {
                            type: 'string',
                            description: 'The HTML ID of the header (returned by webgpu_list_sections).',
                        },
                    },
                    required: ['docType', 'sectionId'],
                },
            },
        ],
    };
});

server.setRequestHandler(CallToolRequestSchema, async (request) => {
    switch (request.params.name) {
        case 'webgpu_list_sections': {
            const docType = String(request.params.arguments?.docType || 'webgpu');
            const query = String(request.params.arguments?.query || '').toLowerCase().trim();

            const $ = await getParsedDoc(docType);

            const results = [];
            $('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]').each((_, el) => {
                const id = $(el).attr('id');
                const text = $(el).text().replace(/\s+/g, ' ').trim();
                if (query) {
                    if (id.toLowerCase().includes(query) || text.toLowerCase().includes(query)) {
                        results.push(`- ${id}: ${text}`);
                    }
                } else {
                    results.push(`- ${id}: ${text}`);
                }
            });

            return {
                content: [
                    {
                        type: 'text',
                        text: results.length > 0
                            ? `Found ${results.length} sections${query ? ` matching "${query}"` : ''} (showing first 500):\n${results.slice(0, 500).join('\n')}`
                            : `No sections found.`,
                    },
                ],
            };
        }

        case 'webgpu_read_section': {
            const docType = String(request.params.arguments?.docType || 'webgpu');
            const sectionId = String(request.params.arguments?.sectionId || '');
            if (!sectionId) throw new Error('sectionId is required');

            const $ = await getParsedDoc(docType);
            const $header = $(`[id="${sectionId.replace(/"/g, '\\"')}"]`).first();

            if ($header.length === 0) {
                return {
                    content: [
                        {
                            type: 'text',
                            text: `Section with ID '${sectionId}' not found. Use webgpu_list_sections to find valid IDs.`,
                        },
                    ],
                    isError: true,
                };
            }

            const tagName = $header.prop('tagName').toLowerCase(); // e.g. "h3"
            const levelMatch = tagName.match(/^h(\d)$/);
            let level = levelMatch ? parseInt(levelMatch[1], 10) : 6;

            let htmlContent = $.html($header);
            let current = $header.next();

            while (current.length > 0) {
                const currentTag = current.prop('tagName').toLowerCase();
                const currentLevelMatch = currentTag.match(/^h(\d)$/);

                if (currentLevelMatch) {
                    const currentLevel = parseInt(currentLevelMatch[1], 10);
                    if (currentLevel <= level) {
                        // We reached a header of same or higher level, stop here
                        break;
                    }
                }

                htmlContent += $.html(current);
                current = current.next();
            }

            const textContent = convert(htmlContent, {
                wordwrap: 100,
                selectors: [
                    { selector: 'a', options: { ignoreHref: true } },
                    { selector: 'img', format: 'skip' }
                ]
            });

            return {
                content: [
                    {
                        type: 'text',
                        text: textContent,
                    },
                ],
            };
        }

        default:
            throw new Error(`Unknown tool: ${request.params.name}`);
    }
});

async function run() {
    const transport = new StdioServerTransport();
    await server.connect(transport);
    console.error('WebGPU MCP server running on stdio');
}

run().catch((error) => {
    console.error('Fatal error in WebGPU MCP server:', error);
    process.exit(1);
});

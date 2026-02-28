import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

async function test() {
  const transport = new StdioClientTransport({
    command: 'node',
    args: ['index.js']
  });

  const client = new Client(
    { name: 'test-client', version: '1.0.0' },
    { capabilities: {} }
  );

  await client.connect(transport);
  
  console.log("Searching sections for 'adapter' in webgpu...");
  const searchResult = await client.callTool({
    name: 'webgpu_list_sections',
    arguments: { docType: 'webgpu', query: 'gpuadapter' }
  });
  
  console.log("Search Result snippet (first 300 chars):");
  console.log(searchResult.content[0].text.substring(0, 300));

  console.log("\nReading 'gpuadapter'...");
  const docResult = await client.callTool({
    name: 'webgpu_read_section',
    arguments: { docType: 'webgpu', sectionId: 'gpuadapter' }
  });
  console.log("\nDoc Fetch Result snippet (first 400 chars):");
  console.log(docResult.content[0].text.substring(0, 400));
  
  process.exit(0);
}

test().catch(console.error);

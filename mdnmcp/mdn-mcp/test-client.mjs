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
  
  console.log("Searching for 'gpuadapter'...");
  const searchResult = await client.callTool({
    name: 'mdn_search_docs',
    arguments: { query: 'gpuadapter' }
  });
  
  console.log("Search Result:");
  console.log(searchResult.content[0].text);

  if (searchResult.content[0].text.includes('web/api/gpuadapter')) {
      const docResult = await client.callTool({
        name: 'mdn_get_doc',
        arguments: { docPath: 'web/api/gpuadapter' }
      });
      console.log("\nDoc Fetch Result snippet (first 300 chars):");
      console.log(docResult.content[0].text.substring(0, 300));
  }
  
  process.exit(0);
}

test().catch(console.error);

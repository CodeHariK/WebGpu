import MarkdownIt from "markdown-it";
import hljs from "highlight.js";
import "highlight.js/styles/atom-one-dark.css";
import { createMemo } from "solid-js";

interface MarkdownProps {
	content: string;
	class?: string;
}

const md = new MarkdownIt({
	html: true,
	linkify: true,
	typographer: true,
	highlight: (str, lang) => {
		if (lang && hljs.getLanguage(lang)) {
			try {
				return hljs.highlight(str, { language: lang }).value;
			} catch (__) {}
		}
		return ""; // use external default escaping
	},
});

export default function Markdown(props: MarkdownProps) {
	const rendered = createMemo(() => md.render(props.content));

	return (
		<div
			class={`markdown-body ${props.class || ""}`}
			innerHTML={rendered()}
			style={{
				"line-height": "1.6",
				color: "#ccc",
				"font-size": "0.95rem",
			}}
		/>
	);
}

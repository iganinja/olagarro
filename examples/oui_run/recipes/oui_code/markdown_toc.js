// For OUI Code's Tools > Interactive Script, on a Markdown document: a table of contents from its headings, put
// between the lines <!-- toc --> and <!-- /toc -->, or under the title when there are none yet.
//
// A script that exports transform gets the document's text and gives back the text in its place. The result shows
// beside the script as it is written, and Apply puts it in the document.

// The anchor GitHub gives a heading: lower case, without punctuation, spaces as hyphens; a second heading with the
// same text gets "-1" after it, a third "-2".
function anchors() {
    const seen = new Map();
    return (heading) => {
        const base = heading.toLowerCase().replace(/[^\p{L}\p{N} _-]/gu, "").replace(/ /g, "-");
        const times = seen.get(base) ?? 0;
        seen.set(base, times + 1);
        return times === 0 ? base : `${base}-${times}`;
    };
}

export function transform(text) {
    const lines = text.split("\n");
    const anchor = anchors();
    const entries = [];
    let fenced = false;
    for (const line of lines) {
        // A "#" inside a code block is not a heading.
        if (/^\s*```/.test(line)) {
            fenced = !fenced;
        }
        const heading = fenced ? null : /^(#{2,4})\s+(.+?)\s*#*$/.exec(line);
        if (heading) {
            const depth = heading[1].length - 2;
            entries.push(`${"  ".repeat(depth)}- [${heading[2]}](#${anchor(heading[2])})`);
        }
    }
    const toc = ["<!-- toc -->", ...entries, "<!-- /toc -->"];

    const start = lines.indexOf("<!-- toc -->");
    const end = lines.indexOf("<!-- /toc -->");
    if (start >= 0 && end > start) {
        lines.splice(start, end - start + 1, ...toc);
    } else {
        // Under the title, or at the top without one.
        const title = lines.findIndex((line) => /^#\s/.test(line));
        lines.splice(title + 1, 0, ...(title >= 0 ? [""] : []), ...toc, ...(title >= 0 ? [] : [""]));
    }
    return lines.join("\n");
}

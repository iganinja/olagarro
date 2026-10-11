// Runs before every other script: helpers they can all use.

// api(colouring): oui.words(...lists: string[][]): RegExp
// A regular expression for any of the words, whole words only, as keyword rules want: oui.words(["if", "else"]).
oui.words = (...lists) =>
    new RegExp("\\b(?:" + lists.flat().map(word => word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|") + ")\\b");

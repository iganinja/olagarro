// Plain JavaScript, no window: oui_run hello.js [name]
// Prints, runs a command whose output goes straight to the terminal, asks a question, and ends when
// everything it started has.
const name = oui.args[0] ?? "world";
console.log(`Hello, ${name}.`);

const code = await oui.run("uname -sr");
console.log(`uname ended with ${code}.`);

if (await oui.confirm("Wait a second before saying goodbye?")) {
    setTimeout(() => console.log("A second later: goodbye."), 1000);
} else {
    console.log("Goodbye.");
}

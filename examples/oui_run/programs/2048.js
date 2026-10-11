// 2048: oui_run 2048.js [SEED], or oui_run_gui --window 2048.js [SEED]
// The arrows slide every tile as far as it goes; two of a number that meet become one of their sum, which scores.
// After each move a 2, or now and then a 4, appears in an empty square. Reach 2048 to win; with no move left, the
// game is over. Alt+N starts a new one and Escape ends the program. A SEED, a number, gives the same game each time.
//
// The board is a description written here, as text: a Grid of labels, each with an id, so the script only changes
// what they say.
const size = 4;
const cell = (row, column) => `c${row}${column}`;

let squares = "";
for (let row = 0; row < size; ++row) {
    for (let column = 0; column < size; ++column) {
        squares += `    Label "" id=${cell(row, column)} bold\n`;
    }
}
const ui = oui.ui(`Window "2048" width=34
  Grid columns=${Array(size).fill("6").join(",")} gap=tight
${squares}  VSpacer height=1
  Row centered gap=tight
    Label "Score: 0" id=score
    Button "&New Game" id=again compact flat
`);

// Random numbers that repeat for a seed, as tests want, and differ each game without one.
let seed = Number(oui.args[0] ?? Date.now()) >>> 0;
function random() {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    return seed / 2 ** 32;
}

let board;
let score;
let won;

function show() {
    for (let row = 0; row < size; ++row) {
        for (let column = 0; column < size; ++column) {
            const value = board[row][column];
            // Six columns each, the number in the middle of them, a dot where there is none.
            const text = value === 0 ? "·" : String(value);
            const left = Math.floor((6 - text.length) / 2);
            ui[cell(row, column)].text = " ".repeat(left) + text;
        }
    }
    ui.score.text = `Score: ${score}`;
}

function addTile() {
    const empty = [];
    for (let row = 0; row < size; ++row) {
        for (let column = 0; column < size; ++column) {
            if (board[row][column] === 0) {
                empty.push([row, column]);
            }
        }
    }
    if (empty.length > 0) {
        const [row, column] = empty[Math.floor(random() * empty.length)];
        board[row][column] = random() < 0.9 ? 2 : 4;
    }
}

function newGame() {
    board = Array.from({length: size}, () => Array(size).fill(0));
    score = 0;
    won = false;
    addTile();
    addTile();
    show();
}

// One line slid towards its start: the numbers closed up, two equal neighbours joined once.
function slide(line) {
    const numbers = line.filter((value) => value !== 0);
    const result = [];
    for (let index = 0; index < numbers.length; ++index) {
        if (numbers[index] === numbers[index + 1]) {
            result.push(numbers[index] * 2);
            score += numbers[index] * 2;
            ++index;
        } else {
            result.push(numbers[index]);
        }
    }
    return result.concat(Array(size - result.length).fill(0));
}

// The board's lines as a move takes them, each read from where the tiles go to.
function lines(direction) {
    const all = [];
    for (let index = 0; index < size; ++index) {
        const positions = [];
        for (let step = 0; step < size; ++step) {
            positions.push(direction === "left" ? [index, step]
                         : direction === "right" ? [index, size - 1 - step]
                         : direction === "up" ? [step, index]
                         : [size - 1 - step, index]);
        }
        all.push(positions);
    }
    return all;
}

function canMove() {
    for (let row = 0; row < size; ++row) {
        for (let column = 0; column < size; ++column) {
            const value = board[row][column];
            if (value === 0 || board[row][column + 1] === value || board[row + 1]?.[column] === value) {
                return true;
            }
        }
    }
    return false;
}

async function move(direction) {
    let moved = false;
    for (const positions of lines(direction)) {
        const slid = slide(positions.map(([row, column]) => board[row][column]));
        positions.forEach(([row, column], index) => {
            moved ||= board[row][column] !== slid[index];
            board[row][column] = slid[index];
        });
    }
    if (!moved) {
        return;
    }
    addTile();
    show();
    if (!won && board.some((line) => line.includes(2048))) {
        won = true;
        await oui.message("2048", `You made 2048, with ${score} points. Play on, or Alt+N for a new game.`);
    } else if (!canMove()) {
        await oui.message("Game over", `No move left: ${score} points.`);
        newGame();
    }
}

for (const direction of ["left", "right", "up", "down"]) {
    oui.shortcut(direction, () => move(direction));
}
ui.again.onClick = newGame;
oui.shortcut("Escape", () => oui.quit());
newGame();
oui.window(ui);

# Calculator

Write an expression as you would on paper and press **Enter**: it goes to the log with what it comes to, and the
line is clear for the next. While you write, what it comes to so far shows under the log.

```
(800*0.16) - (0.32 * 500)       = -32
interes = 800*0.03              interes = 24
interes + 800                   = 824
800 + 16%                       = 928
```

## Writing expressions

| Write | For |
|-------|-----|
| `+ - * /`, `×` `÷` | Adding, taking away, multiplying, dividing |
| `^` or `**` | A power: `2^10`, from the right, so `2^3^2` is `2^9` |
| `( )` | Grouping |
| `5!` | A factorial |
| `√9` | A square root |
| `7 mod 3` | What is left over when dividing |
| `3(4+1)`, `2pi` | A value next to a bracket or a name multiplies it |
| `1e3`, `.5`, `1_000` | Numbers as they are written |
| `0x1F`, `0b101`, `0o17` | Numbers in base 16, 2 and 8 |

`-2^2` is -4: the power first, then the sign, as in mathematics. Two numbers next to each other, as `2 3`, are taken
for a slip, not a product.

## Percentages

`16%` is a hundredth of 16, 0.16, so `800 * 16%` is 128. Added to or taken from a value, a percentage is that share
of it, as a shop's sums are: `800 + 16%` is 928, and `800 - 25%` is 600.

## Keeping values

`name = expression` keeps the value under the name, for the expressions that follow: `interes = 800*0.03`, then
`interes + 800`. Writing it again changes it. Names are letters, digits and `_`, starting with a letter, accented
ones too. `ans` is the last value worked out.

## Functions and constants

| Function | Gives |
|----------|-------|
| `sqrt(x)`, `cbrt(x)` | Square and cube roots |
| `abs(x)`, `sign(x)` | The value without its sign; -1, 0 or 1 |
| `round(x)`, `round(x, digits)` | Rounded, to whole numbers or to so many decimals |
| `floor(x)`, `ceil(x)`, `trunc(x)` | Down, up, and towards zero |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)` | Trigonometry, in radians or degrees |
| `sinh`, `cosh`, `tanh` | Hyperbolic functions |
| `ln(x)`, `log(x)`, `log(x, base)`, `log2(x)`, `log10(x)`, `exp(x)` | Logarithms and powers of e |
| `min(a, b, ...)`, `max(a, b, ...)`, `hypot(a, b, ...)` | The least, the greatest, the length |
| `gcd(a, b)`, `lcm(a, b)` | Greatest common divisor, least common multiple |

The constants are `pi` (or `π`), `e` and `tau`.

## Commands and keys

| Write or press | To |
|----------------|----|
| `deg`, `rad` | Measure angles in degrees or in radians; the corner says which, and a click there switches |
| `clear` | Empty the log |
| `help` | List the functions in the log |
| `exit` | Close the calculator |
| Enter | Work out the line and put it in the log |
| Up, Down | Bring back what was written before |
| F2 | Options: angles in degrees or in radians |
| F1 | This help |

Results show to 15 significant digits, so `0.1 + 0.2` is 0.3. When something is wrong, the line stays as it is, the
cursor goes where the mistake is, and the line under the log says what it is.

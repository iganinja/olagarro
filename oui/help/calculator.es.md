# Calculadora

Escribe una expresión como lo harías en papel y pulsa **Intro**: pasa al registro con su resultado, y la línea queda
libre para la siguiente. Mientras escribes, bajo el registro se ve lo que va dando.

```
(800*0.16) - (0.32 * 500)       = -32
interes = 800*0.03              interes = 24
interes + 800                   = 824
800 + 16%                       = 928
```

## Escribir expresiones

| Escribe | Para |
|---------|------|
| `+ - * /`, `×` `÷` | Sumar, restar, multiplicar, dividir |
| `^` o `**` | Una potencia: `2^10`, de derecha a izquierda, así que `2^3^2` es `2^9` |
| `( )` | Agrupar |
| `5!` | Un factorial |
| `√9` | Una raíz cuadrada |
| `7 mod 3` | El resto de una división |
| `3(4+1)`, `2pi` | Un valor junto a un paréntesis o un nombre lo multiplica |
| `1e3`, `.5`, `1_000` | Números tal como se escriben |
| `0x1F`, `0b101`, `0o17` | Números en base 16, 2 y 8 |

`-2^2` es -4: primero la potencia y luego el signo, como en matemáticas. Dos números seguidos, como `2 3`, se toman
por un despiste, no por un producto.

## Porcentajes

`16%` es la centésima parte de 16, 0,16, así que `800 * 16%` es 128. Sumado a un valor o restado de él, un porcentaje
es esa parte del valor, como en las cuentas de una tienda: `800 + 16%` es 928, y `800 - 25%` es 600.

## Guardar valores

`nombre = expresión` guarda el valor con ese nombre para las expresiones que siguen: `interes = 800*0.03`, y luego
`interes + 800`. Escribirlo otra vez lo cambia. Los nombres son letras, cifras y `_`, empezando por una letra, también
con tilde. `ans` es el último resultado.

## Funciones y constantes

| Función | Da |
|---------|----|
| `sqrt(x)`, `cbrt(x)` | Raíces cuadrada y cúbica |
| `abs(x)`, `sign(x)` | El valor sin signo; -1, 0 o 1 |
| `round(x)`, `round(x, decimales)` | Redondeado, a entero o a tantos decimales |
| `floor(x)`, `ceil(x)`, `trunc(x)` | Hacia abajo, hacia arriba y hacia cero |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)` | Trigonometría, en radianes o en grados |
| `sinh`, `cosh`, `tanh` | Funciones hiperbólicas |
| `ln(x)`, `log(x)`, `log(x, base)`, `log2(x)`, `log10(x)`, `exp(x)` | Logaritmos y potencias de e |
| `min(a, b, ...)`, `max(a, b, ...)`, `hypot(a, b, ...)` | El menor, el mayor, la longitud |
| `gcd(a, b)`, `lcm(a, b)` | Máximo común divisor, mínimo común múltiplo |

Las constantes son `pi` (o `π`), `e` y `tau`.

## Órdenes y teclas

| Escribe o pulsa | Para |
|-----------------|------|
| `deg`, `rad` | Medir los ángulos en grados o en radianes; la esquina dice cuál, y pulsarla los cambia |
| `clear` | Vaciar el registro |
| `help` | Listar las funciones en el registro |
| `exit` | Cerrar la calculadora |
| Intro | Calcular la línea y pasarla al registro |
| Arriba, Abajo | Recuperar lo escrito antes |
| F2 | Opciones: ángulos en grados o en radianes |
| F1 | Esta ayuda |

Los resultados salen con 15 cifras significativas, así que `0.1 + 0.2` es 0,3. Cuando algo está mal, la línea se queda
como está, el cursor va adonde está el error, y la línea bajo el registro dice cuál es.

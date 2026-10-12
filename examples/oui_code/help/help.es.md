# OUI Code

Un editor de texto para la terminal, hecho con OUI. Funciona igual en local que por SSH, y no necesita nada
instalado aparte de sí mismo. Pulsa **Esc** para cerrar esta ayuda; **Izquierda** y **Derecha**, o **Ctrl+Re Pág** y
**Ctrl+Av Pág**, pasan de una parte a otra, y **Re Pág**, **Av Pág**, **Inicio** y **Fin** recorren cada una.

## Moverse

- **F10** abre los menús, y **Alt** con la letra subrayada de un menú abre ese. Algunas terminales se quedan Alt
  y una letra para ellas; F10 llega siempre a OUI Code.
- **F1** abre esta ayuda.
- **Ctrl+L** en una ventana como la de abrir archivo la coloca: las flechas la mueven, Ctrl y las flechas
  cambian su tamaño, Intro la deja ahí y Esc la devuelve. Las ventanas vuelven a abrirse donde las dejaste; con
  el ratón también se arrastran por el título y por los bordes.
- **Ctrl+Mayús+P** abre la paleta de comandos: todos los comandos, con sus teclas. Escribe parte de un nombre
  para acotar la lista, y luego Intro. También está en el menú Ver, para las terminales que mandan Ctrl+Mayús+P
  igual que Ctrl+P.
- **Alt+←** vuelve a donde estaba el cursor antes de saltar, a otro documento o a diez líneas o más, fuera como
  fuera: Ir a la línea, una búsqueda, un clic, un archivo abierto. **Alt+→** vuelve a ir adelante. Un documento
  cerrado entretanto se abre de nuevo.
- **Ctrl+P** va a un archivo del proyecto escribiendo parte de su nombre (ver *Buscar archivos*).
- **F4** pasa el foco por el árbol del proyecto, el editor y el explorador.
- **Ctrl+Av Pág** y **Ctrl+Re Pág** van a la pestaña siguiente y a la anterior, de la última a la primera al
  llegar al final. **Mayús+F4** lista las pestañas abiertas, para elegir una con las flechas e Intro. En
  una ventana, **Ctrl+Tab** y **Ctrl+Mayús+Tab** hacen lo mismo que Ctrl+Av Pág y Ctrl+Re Pág.
- Una pestaña muestra el nombre del archivo; la barra de estado muestra su ruta completa, y también la muestra
  dejar el puntero quieto sobre la pestaña.
- **Ctrl+K** divide el editor: la línea de abajo muestra las teclas, y una flecha abre un grupo de pestañas
  nuevo a ese lado del grupo actual, que se lleva la pestaña actual si su grupo tiene más. Se puede dividir
  una y otra vez, tantos grupos como quepan en la pantalla. Tras Ctrl+K, Mayús y una flecha llevan en cambio
  la pestaña actual al grupo de ese lado, Alt y las flechas mueven la línea junto al grupo tantas veces como
  quieras hasta pulsar Intro, y Retroceso fusiona el grupo con el de al lado, que se queda con sus pestañas.
  **Alt+K** sirve para moverse por la ventana: cada flecha lleva a lo más cercano de ese lado, otro grupo, una parte
  de la pestaña (el código y el resultado de un script o una interfaz interactivos) o un panel lateral, y Re Pág
  y Av Pág van a la pestaña anterior o siguiente; cualquier otra tecla acaba y hace lo suyo, así que puedes
  ponerte a escribir directamente. Funciona también desde una pestaña de terminal, que se queda las demás teclas
  para sus programas. Un grupo que se queda sin pestañas se cierra. Los
  comandos actúan en el grupo que tiene el foco; F4 pasa por todos, y las líneas entre ellos se pueden
  arrastrar.
- **F9** abre la vista previa de un documento Markdown en una pestaña propia, que sigue al documento mientras
  escribes; F9 en la vista previa vuelve al documento. Sus enlaces se siguen con un clic, o con **Tab** hasta uno e
  **Intro**: otro archivo Markdown se lee en la vista previa, como en un navegador, y **Alt+Izquierda** y
  **Alt+Derecha** van atrás y adelante; una sección de uno va a su título; una dirección web se abre en el navegador.
  **Ctrl+Mayús+E** abre para editarlo el archivo que enseña la vista previa.

- **Ctrl+T** abre una pestaña nueva, de lo que vaya a tener: un archivo nuevo, una terminal, la calculadora, un
  script o una interfaz interactivos, la comparación de dos archivos, la lista de tareas o los TODO del código.

## Proyectos y archivos

Abrir una carpeta (**Archivo > Abrir carpeta**, una carpeta reciente u `oui_code carpeta`) la convierte en el
*proyecto*. El **árbol del proyecto**, a la izquierda, la muestra y nunca sube por encima de ella; **Ctrl+B** lo
muestra u oculta. El **explorador**, a la derecha (**Ctrl+E**), recorre todo el sistema de archivos, para traer
cosas al proyecto. Sus cabeceras dicen cuál es cuál, y el que tiene el foco se distingue.

OUI Code vuelve como se dejó, pestañas incluidas, se arranque como se arranque. Los archivos que se le nombran al
arrancarlo, `oui_code notas.txt tareas.md`, se abren encima, el último delante; uno que aún no existe se abre
vacío con su nombre, y guardarlo lo crea.

Los dos paneles admiten las mismas teclas:

| Tecla | Acción |
|-------|--------|
| Intro o un clic | Abrir un archivo, o entrar en una carpeta |
| Retroceso | Subir una carpeta |
| Escribir | Saltar a la entrada cuyo nombre empieza así |
| Insert | Archivo nuevo, que se abre en una pestaña |
| F7 | Carpeta nueva |
| F2 | Renombrar |
| Supr | Borrar, tras preguntar |
| F5 | Copiar a la carpeta que muestra el otro panel |

Un clic derecho en una entrada abre un menú con lo mismo: archivo o carpeta nuevos, renombrar, borrar y copiar. En un
archivo ofrece además **Abrir como**, como texto o como lista de tareas, por ejemplo el Markdown de una lista para
editarlo a mano; un archivo solo se abre como lista de tareas si no tiene nada más, para no perder nada de él.
La línea del borde interior de cada panel se arrastra para ensancharlo o estrecharlo, y se queda así.

El proyecto, los paneles, la carpeta del explorador y los grupos del editor con sus archivos vuelven como los
dejaste cuando OUI Code arranca sin nada que abrir.

## Buscar archivos

**Ctrl+P** lista los archivos del proyecto. En un repositorio git deja fuera lo que git ignora, según sus
archivos .gitignore; fuera de git, los archivos y carpetas ocultos y las carpetas de compilación. Lo que
escribes los acota:

- Las letras sin más buscan de forma *aproximada*: las letras en orden, no necesariamente juntas. `tblcpp`
  encuentra `table.cpp`. Pesan más las letras que empiezan palabra y las que caen en el nombre del archivo.
- Un patrón con `*` o `?` funciona como en la terminal: `*.h` encuentra todas las cabeceras, `hola*.h` las
  cabeceras cuyo nombre empieza por "hola", `*edit*` cualquier nombre que contenga "edit". Con una `/` el patrón
  abarca la ruta entera, como en `src/*.h`.

## Editar

| Teclas | Acción |
|--------|--------|
| Ctrl+Z / Ctrl+Y | Deshacer / rehacer |
| Ctrl+C, Ctrl+X, Ctrl+V | Copiar, cortar, pegar; también Ctrl+Insert, Mayús+Supr, Mayús+Insert |
| Ctrl+A | Seleccionar todo |
| Ctrl+Izquierda / Ctrl+Derecha | Moverse por palabras |
| Tab / Mayús+Tab | Sangrar o quitar sangría; con varias líneas, a todas |
| Alt+Arriba / Alt+Abajo | Subir o bajar la línea |
| Ctrl+Mayús+D | Duplicar la línea |
| Ctrl+Mayús+K | Borrar la línea |
| Ctrl+/ | Comentar o descomentar las líneas; Ctrl+7 en teclado español |
| Ctrl+Mayús+M | Ir al paréntesis o llave pareja del que está junto al cursor |
| Mayús+Alt+Arriba / Mayús+Alt+Abajo | Añadir un cursor en la línea de arriba o de abajo |

Con varios cursores, escribir, borrar, moverse, copiar y pegar ocurre en todos. Pegar tantas líneas como
cursores hay da a cada cursor la suya. Esc o un clic dejan un solo cursor.

El paréntesis o la llave junto al cursor y su pareja se distinguen, estén donde estén: OUI Code todavía no
distingue las cadenas y los comentarios del código.

Al seleccionar texto dentro de una línea se marcan los demás sitios del documento con el mismo texto, letra por
letra, como dónde más se usa un nombre.

Mientras escribes, OUI Code sugiere cómo puede seguir la palabra en cuanto tiene dos letras: las palabras de los
documentos abiertos, lo que ofrece el lenguaje del documento, como sus palabras clave y trozos de texto para un
bucle o una función, y en un script (`.js`) lo que pueden usar los scripts de OUI, tras `oui.` con cómo se llama
cada cosa, y tras un nombre hecho con `new oui.Label(...)` u otra de sus clases, lo que tiene. Arriba y Abajo eligen, Intro o Tab la ponen en lugar de la palabra, Esc cierra la lista y seguir
escribiendo la estrecha. **Ctrl+Espacio**, o **Alt+/** en una terminal que se quede Ctrl+Espacio, pide sugerencias
en cualquier momento, y avisa cuando no hay nada que sugerir. En Preferencias se desactiva la lista que se abre
sola.

En un script, dentro de los paréntesis de una llamada a algo que pueden usar los scripts de OUI, como
`oui.writeFile(`, una nota sobre la línea muestra cómo se llama, con el parámetro que estás escribiendo marcado, y
qué hace. Sigue al cursor de un argumento al siguiente y se va al cerrar los paréntesis; Esc la cierra hasta que
estés en otra llamada.

## Buscar

- **Ctrl+F** abre la barra de búsqueda, con lo último que buscaste seleccionado para que lo que escribas lo
  sustituya. Las coincidencias se resaltan mientras escribes y las cuenta ("3 de 12"), y la primera desde el cursor
  queda marcada y a la vista. Intro va a la siguiente y entra en el documento ahí; **F3** y las flechas de al lado de la barra pasan de una a
  otra. Sus opciones: palabra completa, expresión regular, distinguir mayúsculas.
- **Ctrl+F3** busca lo seleccionado, o la palabra del cursor si no hay nada seleccionado, y va al siguiente sitio
  donde está, sin abrir la barra; luego **F3** y **Mayús+F3** pasan de uno al siguiente.
- **Ctrl+H** reemplaza, de una en una o todas a la vez.
- **Ctrl+Mayús+F** busca en todos los archivos del proyecto, con las mismas opciones que la barra de búsqueda,
  y solo en los que nombra un patrón si se da uno, como `*.cpp *.h`. Las coincidencias llenan una pestaña
  propia en cuanto acaba la búsqueda, que sigue mientras trabajas; Intro en una abre su archivo allí. Los
  archivos abiertos con cambios sin guardar se buscan tal como los tienes. Quedan fuera los binarios y los de
  más de 16 MB, y solo se listan las 2000 primeras coincidencias.
- **Ctrl+G** va a una línea.

**Herramientas > Filtrar por un programa** ejecuta una orden del shell con la selección, o el documento entero si no
hay selección, como entrada, y pone lo que imprime en su lugar, como un paso de deshacer: `sort -u`, `jq .`,
`clang-format`, `column -t`. Una orden que falla deja el texto como estaba y enseña su error.

## Comparar archivos

**Herramientas > Comparar archivos...** compara el documento que editas con un archivo que eliges, o dos archivos
elegidos uno tras otro si no hay ningún documento abierto; **Herramientas > Comparar con lo guardado** compara el
documento con su archivo tal como se guardó por última vez, para ver lo que vas a guardar. `oui_code --diff izquierdo
derecho` abre una comparación desde la línea de órdenes, como puede pedirla `git difftool`.

La comparación es una pestaña propia, con los dos textos lado a lado y las líneas iguales una frente a otra. Una línea
que solo está a la izquierda va en rojo, quitada; solo a la derecha, en verde, añadida; en los dos pero distinta, se
distingue de las demás, con lo que cambió dentro de ella en rojo a la izquierda y en verde a la derecha. Un documento
abierto se compara tal como lo tienes, con los cambios sin guardar, y la comparación lo sigue mientras editas.

| Tecla | Acción |
|-------|--------|
| F7 / Mayús+F7 | Diferencia siguiente / anterior |
| Arriba, Abajo, RePág, AvPág, Inicio, Fin | Moverse por las filas |
| Izquierda, Derecha | Ver más de las líneas largas |
| Intro, o doble clic | Ir a esa línea, en su documento; del lado pulsado, o del derecho si no |

La barra bajo los textos dice cuántas diferencias hay, y en cuál estás.

## Scripts

OUI Code colorea el código y transforma el texto con scripts en JavaScript. Los suyos definen C++, JavaScript y
TypeScript, Python, GDScript, JSON, Markdown, CMake y scripts de shell, los comentarios de una veintena de
lenguajes más, y los comandos de **Herramientas > Comandos de texto**: ordenar líneas, quitar las repetidas,
mayúsculas y minúsculas, snake_case y camelCase, formatear y compactar JSON. Cada uno toma la selección, o el
documento entero si no hay selección. **Alt+X** los lista solos, para elegir uno escribiendo parte de su nombre,
como en la paleta.

Cada documento se colorea como el lenguaje de su archivo; **Ver > Sintaxis** elige otro, o texto sin formato, y
la barra de estado dice cuál.

Los tuyos van en la carpeta `oui_code_scripts`, junto a los ajustes: cada archivo `.js` de ahí se ejecuta
después de los de OUI Code, por orden de nombre. **Herramientas > Scripts de usuario > Abrir el script de usuario**
abre `user.js`, que se crea la primera vez con ejemplos, y **Recargar los scripts de usuario**, a su lado, los vuelve
a ejecutar tras un cambio. Un script que falla lo dice, y por qué, al cargarse. Lo que escriben con `console.log`
está en **Salida de los scripts de usuario**, a su lado; un script ejecutado desde Herramientas tiene su propia
pestaña para eso.

Un comando recibe texto y devuelve lo que va en su lugar, o nada para dejarlo como estaba:

```js
oui.commands.register({
    id: "user.shout",
    title: "Gritar",           // o {en: "Shout", es: "Gritar"}
    keys: "Ctrl+Alt+U",        // opcional
    menuPath: "edit/Mayúsculas", // opcional: Edición > Mayúsculas > Gritar
    run: text => text.toUpperCase(),
});
```

Un comando está en **Herramientas > Comandos de texto**, o donde diga su `menuPath`, escrito como una ruta de
archivos. Su primer nombre es uno de los menús, `file`, `edit`, `view`, `tools` o `help`, en el idioma que esté OUI
Code; el resto son submenús, que se crean la primera vez que se nombran y se ven tal como se escriben. Un submenú de
los de OUI Code se encuentra por su nombre en inglés o tal como se ve: `tools/Comandos de texto` añade a ese. Como el
título, puede ser uno por idioma: `{en: "edit/Case", es: "edit/Mayúsculas"}`. Esté donde esté, **Alt+X** y la paleta
también lo listan.

Un lenguaje dice qué archivos son suyos, qué empieza un comentario, qué llaves van emparejadas y cómo
colorearlo:

```js
oui.languages.register({
    id: "ini",
    name: "INI",
    extensions: [".ini"],      // files: ["Makefile"] para nombres completos
    comment: ";",
    states: {
        start: [
            {match: /;.*/, kind: "comment"},
            {match: /^\s*\[.*\]/, kind: "heading"},
            {match: /"/, push: "string"},
        ],
        string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}]},
    },
});
```

El coloreado recorre cada línea por estados, empezando en `start`; en cada punto gana la regla cuyo patrón
encaja antes, y en caso de empate la primera de la lista. Una regla puede entrar en otro estado con `push`,
como una cadena o un comentario, y salir con `pop`; un estado puede durar muchas líneas, como un comentario de
bloque. Los tipos son keyword, type, function, string, number, constant, comment, preprocessor, operator, tag,
attribute, heading, emphasis, link e invalid. `oui.words("if", "else")` da el patrón de una lista de palabras
completas. `suggestions` dice qué sugiere el editor mientras se escribe en el lenguaje: palabras, o `{label,
detail, text}` para un trozo de texto, con `$0` donde va el cursor después. Una definición con el id de otra la
sustituye, así que las tuyas pueden cambiar las de OUI Code. Todo lo que pueden usar los scripts está en
`oui/oui.d.ts`, junto a las fuentes de OUI Code.

### Ejecutar un script

Cualquier script tuyo, esté donde esté, puede trabajar sobre el documento que editas o por su cuenta, por
ejemplo sobre una carpeta entera. Es un módulo que exporta una de dos funciones, a las que se les dice si es un
ensayo:

```js
// Sobre el documento: file es su ruta completa.
export function processFile(file, dryRun) {
    const text = oui.readFile(file);
    const tidy = text.replaceAll("\t", "    ");
    if (dryRun) {
        console.log(`cambiaría ${text.split("\t").length - 1} tabuladores`);
    } else {
        oui.writeFile(file, tidy);
    }
}

// Por su cuenta: folder es la del proyecto, o la del documento si no hay proyecto.
export async function process(dryRun, folder) {
    for (const name of oui.listDir(folder).filter((name) => name.endsWith(".md"))) {
        console.log(dryRun ? "revisaría" : "revisando", name);
        if (!dryRun) await oui.run(`markdownlint "${folder}/${name}"`);
    }
}
```

Un ensayo no debería cambiar nada y sí decir lo que haría; `dryRun` no es opcional, y un script cuya función
no lo recibe se rechaza. **Herramientas > Plantillas de scripts** escribe un script de cualquiera de los dos
tipos donde está el cursor, para empezar desde ahí, y uno de un tercer tipo, que se ejecuta tras Generar en una
pestaña de interfaz interactiva.

**Herramientas > Ejecutar un script...** pregunta qué script, empezando en la carpeta del documento, comprueba
que sea de uno de estos tipos y lo recuerda para la próxima vez; cargarlo para comprobarlo ejecuta lo que el
módulo hace al cargarse, que en scripts como los de arriba es solo definir sus funciones. Después lo ejecuta en
ensayo. **F5** repite el script recordado en ensayo, y **Mayús+F5** de verdad. Un `processFile` en ensayo
trabaja sobre una copia del documento, con los cambios sin guardar incluidos, así que incluso uno que escriba
de todos modos deja el archivo en paz, y lo que ha hecho con la copia se abre en otra pestaña; de verdad,
el documento se guarda antes y su pestaña muestra luego lo que el script cambió. Un `process` no tiene copia
sobre la que trabajar: su ensayo es tan seguro como lo haga el script. Tras uno de verdad, los documentos
abiertos y los paneles de carpetas muestran lo que cambió.

Una pestaña enseña lo que el script escribe y lo que dicen los programas que lanza, con un botón para
pararlos; la de un ensayo se titula *Vista previa*. Cada ejecución reutiliza esa misma pestaña, y F5 o Mayús+F5
pulsados en ella repiten el script sobre el mismo documento. Cerrar la pestaña para la ejecución. La ejecución
termina cuando la función vuelve, o cuando se resuelve la promesa de una `async`.

Además de JavaScript, un script tiene `oui.readFile(path)`, `oui.writeFile(path, text)`, `oui.exists(path)`,
`oui.listDir(path)`, `await oui.run(command, {cwd, input, output})`, `await oui.message(title, text)`,
`await oui.confirm(question)`, `await oui.prompt(title, label, text)`, `await oui.choose(title, options)`,
`setTimeout` y `setInterval`, y puede importar con `import` los módulos que tenga al lado.

### Herramientas propias

Un script en la carpeta `tools`, dentro de `oui_code_scripts`, es una herramienta: un comando tuyo, en los menús y
con sus teclas, que trabaja sobre lo que tiene el editor, con todo lo que tiene un script ejecutado desde
Herramientas. Exporta `tool`, que dice qué es, y `run`, que recibe el archivo y el lenguaje del documento, la carpeta
del proyecto, la selección, y la línea y la columna del cursor:

```js
// oui_code_scripts/tools/count_words.js
export const tool = {
    title: "Count Words",      // o {en: "Count Words", es: "Contar palabras"}
    keys: "Ctrl+Alt+W",        // opcional
    menuPath: "edit",          // opcional: sin él, en Herramientas > Herramientas de usuario
};

export async function run({file, language, folder, selection, line, column}) {
    const words = selection.split(/\s+/).filter(Boolean).length;
    await oui.message("Count Words", `${words} words selected`);
}
```

`title`, `keys` y `menuPath` son como los de un comando de texto. El documento se guarda antes de lanzar la
herramienta, así que su archivo dice lo mismo que el editor; `file` es `null` en uno que nunca se ha guardado,
`selection` es `""` cuando no hay nada seleccionado, y `line` y `column` cuentan desde 1. El texto que devuelve `run`
sustituye la selección, o entra en el cursor, un cambio que deshacer quita; si no devuelve nada, el documento se
queda como está. Lo que escribe, y lo que dicen los programas que lanza, abre la pestaña de los scripts, con su botón
para pararlos; una herramienta que no dice nada no abre nada. Después, los documentos abiertos y los paneles de
carpetas muestran lo que cambió.

Las herramientas se cargan con el resto de tus scripts, y **Recargar los scripts de usuario** recoge una nueva, o un
cambio en lo que dice su `tool`; cada ejecución vuelve a cargar el script, así que un cambio en `run` no necesita
recargar. **Herramientas > Plantillas de scripts** escribe una para empezar.

## Scripts interactivos

**Herramientas > Script interactivo** abre una pestaña con un script a la izquierda y, a la derecha, lo que hace, que
se vuelve a ejecutar un momento después de cada cambio en el script o en aquello sobre lo que trabaja. Desde un script
(`.js`), pregunta sobre qué ejecutarlo: otro documento abierto, texto pegado en la pestaña, que sale encima del
resultado para escribirlo o pegarlo, o nada, para un script que solo imprime. Desde
cualquier otro documento, empieza un script nuevo con un ejemplo, que trabaja sobre ese documento. Coloca la pestaña
como quieras: junto a las demás, en un grupo propio, sola.

- Un script que exporta `transform(text)` recibe el texto del documento y devuelve el texto que va en su lugar: el
  resultado. **Aplicar** lo pone en el documento, un cambio que se deshace; hasta entonces el documento no se toca.
- Un `processFile(file, dryRun)` trabaja sobre una copia del documento, en ensayo, y el resultado es cómo deja la
  copia; un `process(dryRun, folder)` también se ejecuta en ensayo, y lo que haría se ve en lo que imprime.
- Lo que imprime está bajo el resultado; **Log** lo muestra o lo oculta.
- Cuando falla, el error sale abajo, con su línea, a la que lleva un clic; el resultado se queda como estaba hasta
  que el script vuelve a funcionar, marcado como de antes del error.
- Solo mira, porque se ejecuta a medio escribir: leer archivos y carpetas funciona; escribir archivos, ejecutar
  programas y preguntar, no. Ejecútalo aparte, con Herramientas > Ejecutar un script, para hacerlo de verdad.
- El código es un documento como cualquier otro: **Ctrl+S** lo guarda, en el script del que salió.

## Interfaces interactivas

**Herramientas > Interfaz interactiva** abre una pestaña con la descripción de una interfaz a la izquierda y, a la
derecha, la interfaz que describe, construida de nuevo un momento después de cada cambio, para probarla como se usaría:
escribir en sus campos, marcar sus casillas, pulsar sus botones. Sobre un archivo `.oui` trabaja con ese archivo;
desde cualquier otra cosa empieza una descripción nueva a partir de un ejemplo.

Una descripción es un widget por línea: su tipo, su texto entre comillas y luego sus propiedades, `nombre=valor` o un
nombre solo. Lo que va dentro de un widget se sangra debajo de él, y `#` empieza un comentario.

```
Window "Rename Files" width=50
  Label "&Pattern:" buddy=pattern
  LineEdit id=pattern placeholder="A regular expression"
  CheckButton "&Show what changes first" checked
  Row centered gap=tight
    Button "&Rename" default
    Button "Cancel"
```

- `Window`, `Column`, `Row`, `Grid`, `Split` y `Tabs` con `Tab` contienen otros; `Label`, `LineEdit`, `TextEdit`,
  `MarkdownView`, `CheckButton`, `Button`, `ComboBox`, `VSpacer`, `HSpacer` y `Slot`, no. Un `Slot` es donde un
  programa o un script pone lo que una descripción no puede escribir, como una lista; aquí muestra su id.
- `id=nombre` es cómo el código encuentra un widget; `&` marca la letra que va con Alt; `buddy=id` es el campo al que
  lleva la letra de una etiqueta. Una etiqueta sin buddy no tiene a dónde llevarla, y muestra su texto sin la `&`;
  `&&` escribe una.
- Una descripción que dice `context=` traduce sus textos, como las ventanas del propio OUI Code; una que no, los
  muestra tal como están escritos, como la de un script.
- Al escribir, las sugerencias dicen lo que cabe: los widgets al principio de una línea, tras un widget y un espacio
  las propiedades que admite, tras un = sus valores; Ctrl+Espacio las pide en cualquier sitio.
- Cuando no se construye, la línea de abajo dice dónde y por qué, y un clic lleva allí; lo último que se construyó se
  queda hasta que vuelve a construirse.
- La descripción es un documento como cualquier otro: **Ctrl+S** la guarda. Un script la lee con `oui.uiFromFile`,
  como cuenta la guía de scripting.
- **Generar**, sobre la vista previa, escribe en una pestaña nueva código para empezar desde él: el archivo de la
  descripción cargado, cada widget con id conectado a lo que suele hacer falta (el clic de un botón, el cambio de un
  campo) y la ventana abierta. Una descripción que aún no tiene archivo se guarda antes, porque el código la carga por
  su nombre. En el lenguaje que tiene al lado, que se recuerda: **JavaScript**, un script para guardar junto a la
  descripción y ejecutar con oui_run, o **C++**, una función para meter en un programa, con el archivo incrustado por
  `olagarro_embed_files` como dice el comentario de encima. Con el teclado, los dos están en la paleta de comandos.
- Al lado de Generar, un script tuyo puede ejecutarse después, uno por lenguaje, que se recuerda: recibe lo que
  Generar ha escrito, para guardarlo donde lo quiera tu proyecto, compilar con ello o revisarlo, según esté montado tu
  proyecto. Es un módulo que exporta `processGeneratedCode(code, {language, file, dryRun})`, con `language`
  `"javascript"` o `"cpp"` y `file` la ruta completa de la descripción, y tiene todo lo que tiene un script ejecutado
  desde Herramientas (`oui.writeFile`, `await oui.run`...). Lo que imprime, y lo que dicen los programas que ejecuta,
  sale en la pestaña de los scripts ejecutados, y lo que devuelve, como texto, al final: si ha ido bien, qué falta.
- **Nuevo...**, en la misma lista, escribe uno: una pestaña de script interactivo con un script para empezar, que
  escribe el código junto a la descripción salvo que ya haya un archivo, ejecutado a medida que lo cambias sobre lo
  que Generar escribe ahora, pegado en la pestaña. Ahí se ejecuta en ensayo, con `dryRun` a true, y dice lo que
  haría; tras Generar, `dryRun` es false y lo hace. Guardado, es el script que se ejecuta tras Generar en ese
  lenguaje.

  ```js
  export async function processGeneratedCode(code, {language, file, dryRun}) {
      const target = file.replace(/\.oui$/, ".cpp");
      if (dryRun) return `Escribiría ${target} y compilaría.`;
      oui.writeFile(target, code);
      const exit = await oui.run("cmake --build build");
      return exit === 0 ? "Compilado." : "La compilación ha fallado: mira arriba.";
  }
  ```

## Cosas por hacer

**Ver > TODO del código** lista todos los TODO, FIXME, XXX y HACK de los archivos del proyecto, como Buscar en
archivos lista lo que encuentra; Intro en uno abre su archivo allí.

**Ver > Lista de tareas** lleva la lista propia del proyecto de cosas por hacer, cada una con una línea que dice qué
y una nota con todo lo que haga falta, como un ticket. La lista está a la izquierda y la nota de la tarea marcada a
la derecha:

| Tecla | Acción |
|-------|--------|
| Intro, Insert o el botón **+ Añadir tarea** | Tarea nueva, tras la marcada, con su prioridad |
| Espacio, o un clic en su casilla | Hecha, o pendiente otra vez: pasa a la otra pestaña |
| F6 | Ver las tareas resueltas, o las pendientes |
| F2 | Renombrar |
| Alt+1 a Alt+4 | Prioridad urgente, alta, normal o baja |
| Supr | Borrar, tras preguntar |
| Alt+Arriba / Alt+Abajo | Subirla o bajarla |
| Tab, o un doble clic | Escribir su nota; Esc vuelve a la lista |
| Escribir, o Ctrl+F | Buscar tareas por su título o su nota, en el campo sobre la lista |

Sobre la lista, las pestañas **Pendientes** y **Resueltas** dicen cuántas hay de cada. Las pendientes se muestran en tu
propio orden y las resueltas de la más reciente a la más antigua, hasta que un clic en la cabecera **Nombre** o
**Prioridad** las ordena por ella, hacia arriba, hacia abajo y de vuelta; mover tareas con Alt+Arriba y Alt+Abajo cambia
tu orden, así que funciona cuando la lista de pendientes lo muestra entero y sin ordenar. Un clic derecho en una tarea abre un menú con lo mismo que las teclas. Sobre la nota,
**Título** y **Prioridad** muestran y cambian qué es la tarea y su urgencia; la lista muestra la casilla, el nombre y la
prioridad de cada tarea en columnas.

Cada cambio se guarda al momento en `tasks.md`, en la raíz del proyecto, en Markdown, para leerlo o editarlo a mano
o llevarlo en git: una línea `- [ ] título` por tarea, `- [x]` cuando está hecha seguido del día en que se hizo, su prioridad delante del título como en todo.txt, `(A)`
urgente, `(B)` alta y `(D)` baja, sin nada si es normal, y su nota debajo con dos espacios de sangría. Sin proyecto, la lista se guarda junto a los ajustes. **Guardar como** lleva una lista a cualquier otro
sitio, con un nombre acabado en `.tasks.md`; abrir un archivo llamado `tasks.md` o acabado así lo abre de nuevo
como lista de tareas. Las listas de tareas vuelven con el resto de la sesión.

## Una shell en una pestaña

**Herramientas > Nueva terminal**, o **Ctrl** o **Alt** y la tecla bajo Esc (**Ctrl+º** o **Alt+º** en un teclado
español, **Ctrl+`** o **Alt+`** en uno inglés), abre tu shell en una pestaña, en la carpeta abierta; escribir
`exit` la cierra. En un terminal solo vale Alt: un terminal no tiene manera de mandar Ctrl con esa tecla. Mientras tiene el foco se queda las teclas que quiere un programa de terminal, Ctrl+C y Ctrl+W
entre ellas, salvo **Ctrl+RePág**, **Ctrl+AvPág** y **Ctrl+Tab**, que siguen moviéndose entre pestañas, y
**Ctrl+Mayús** con una letra, que sigue siendo de OUI Code: **Ctrl+Mayús+V** pega. **Mayús+RePág** y
**Mayús+AvPág**, y la rueda, miran atrás lo que se fue por arriba. Arrastrar el ratón selecciona, un doble clic
coge una palabra y un triple clic una línea; lo seleccionado se copia al soltar el botón, y con **Ctrl+Mayús+C**.
El clic derecho pega.

## Guardar y no perder trabajo

- **Ctrl+S** guarda, **Ctrl+Mayús+S** guarda como, y **Archivo > Guardar todo** guarda todas las pestañas con
  cambios.
- OUI Code se da cuenta cuando otro programa cambia un archivo abierto, y lo recarga o pregunta, según digan las
  Preferencias.
- Si la terminal se cierra o se corta la conexión con cambios sin guardar, se conservan y vuelven, todavía sin
  guardar, la próxima vez que arranca OUI Code.
- Cada pocos segundos OUI Code guarda además una copia de los cambios sin guardar, así que un cuelgue o un
  apagón no pierden más que eso. En Preferencias se desactiva o se cambia cada cuánto; si no, cada 10 segundos.
- Con **Al salir, conservar los cambios sin guardar** activado en Preferencias, salir no pregunta nada y todo
  vuelve como estaba, como en Sublime Text.
- Los ajustes, `oui_code.ini`, y lo que se conserva del trabajo sin guardar están junto al propio OUI Code cuando
  se puede escribir en su carpeta, así que viajan con él en un USB; si no, como cuando está instalado para todo el
  sistema, en tu carpeta de configuración: `~/.config/oui_code` en Linux, `%APPDATA%\oui_code` en Windows.
- **Ver > Tema** elige el aspecto también en la terminal: el azul de Turbo C++, Noche, Claro, o Monocromo para una
  terminal de pocos colores. Se guarda para la próxima vez.

## En una ventana

OUI Code en su variante GUI se abre también en una ventana del escritorio: desde el menú del escritorio, o
escribiendo `oui_code --window` en una terminal. Todo funciona como en la terminal, y le llegan las teclas que una terminal se queda para
sí. **Ver > Tema** elige su aspecto, y **Ctrl++**, **Ctrl+-** y **Ctrl+0** agrandan el texto, lo encogen o lo dejan
como estaba; las dos cosas se guardan para la próxima vez. Las pestañas se pueden arrastrar por su título, a otro
sitio de su grupo o a otro grupo. Escrito en una terminal sin `--window`, funciona en la terminal, como
la variante de terminal.

## Terminales

Algunas combinaciones de teclas nunca llegan a un programa que corre en una terminal:

- **Ctrl+M**, **Ctrl+I**, **Ctrl+J** y **Ctrl+[** llegan como Intro, Tab, un salto de línea y Esc. Las
  terminales que no distinguen Ctrl+Mayús y una letra de Ctrl y la letra mandan también **Ctrl+Mayús+M** como
  Intro; el comando está además en el menú Editar.
- Una terminal puede quedarse **Alt** y una letra, o **Ctrl+Mayús** y una letra, para sus propios menús. Konsole
  en español se queda Alt+A, por ejemplo; usa F10 para llegar a los menús.
- Las teclas del teclado numérico pueden perder el Ctrl.

En la consola de Linux, OUI Code se adapta a sus 8 colores de fondo y a su fuente. Para ver en cualquier terminal
cómo se ve allí, arráncalo como `OUI_LINUX_CONSOLE=1 oui_code`.

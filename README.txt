# SkyChartExporter Plugin

## Introducción

**SkyChartExporter** es un plugin cartográfico avanzado para Stellarium, diseñado específicamente para la publicación científica. Su función principal es transformar las simulaciones visuales del firmamento en **planos celestes cartográficos de calidad editorial e imprenta** (en formatos vectoriales y mapas de bits de alta densidad), complementando las capturas de pantalla rasterizadas existentes de forma nativa.

Esta herramienta permite a los investigadores generar láminas de atlas astronómicos en fondos blancos limpios, con control milimétrico de márgenes, sistemas de retículas ecuatoriales y azimutales, rosa de los vientos dinámica, leyendas analíticas de magnitudes y tipo espectral y la capacidad de destacar astros o constelaciones de relevancia con tipografías personalizadas;  además de permitir personalizar titulo y un subtitulo del plano o imagen.

---

## Índice

1. [Instalación]
* 1.1 [Prerrequisitos]
* 1.2 [Activación del Plugin en Stellarium]


2. [Primeros Pasos]
* 2.1 [Interfaz de Usuario]
* 2.2 [Configuración de una Exportación Paso a Paso]
* 2.3 [Interpretación y Composición de la Lámina Cartográfica]


3. [Metodología Científica del Plugin]
* [Jerarquía de Brillo y Cuerpos Hiperbrillantes]
* [El Caso Especial de la Fase Lunar y Orientación Solar]
* [Cuadrículas de Coordenadas y Corte de Horizonte Topográfico]


4. [Estructura de Archivos y Exportación]
* [Perfil Imprenta y Edición Vectorial (PDF / SVG)]
* [Perfil Digital y Publicación Web (PNG / JPG)]


5. [Mejores Prácticas para Arqueólogos]

6. [Créditos]
7. [Licencia]

---

## 1. Instalación

El plugin **SkyChartExporter** se integra en el entorno de desarrollo y compilación de Stellarium 26.1 como un módulo estándar, comunicándose de forma nativa con sus catálogos internos.

### 1.1 Prerrequisitos

* **Stellarium:** Versión 26.1 requiere Qt6.


### 1.2 Activación del Plugin en Stellarium

1. Inicia Stellarium.
2. Abre la ventana de **Configuración** (`F2`) y dirígete a la pestaña **Plugins**.
3. Busca **Sky Chart Exporter** en la lista lateral izquierda.
4. Marca la casilla **"Cargar al inicio"** (*Load at startup*).
5. Reinicia Stellarium para que los cambios surtan efecto.
6. Tras reiniciar, se visualizará el botón de **SkyChartExporter** en la barra de herramientas inferior de Stellarium.

---

## 2. Primeros Pasos

### 2.1 Interfaz de Usuario

Al pulsar el botón del plugin en la barra de herramientas, se abrirá el diálogo principal `SkyChartExporterDialog`. Esta ventana está organizada en tres pestañas temáticas y una sección fija de control:

```
+---------------------------------------------------------------------------------------+
|  [ Config. Formato ]       [ Config. Estrellas ]       [ Config. Gráficos ]           |
+---------------------------------------------------------------------------------------+
|                                                                                       |
|  ÁREA DINÁMICA DE PARÁMETROS SEGÚN LA PESTAÑA SELECCIONADA                            |
|                                                                                       |
+---------------------------------------------------------------------------------------+
|  [ Mostrar vista previa ]          [ Exportar ]                    [ Cancelar ]       |
|  Estado: Listo                     [========================  ] (Barra de progreso)   |
+---------------------------------------------------------------------------------------+

```

* **Pestaña 1: Configuración de Formato (Dinámica)**
* *Selector Superior:* Conmuta entre **PDF** (Perfil Vectorial/Imprenta) e **Imagen** (Perfil Digital/Web).


* *Configuración de Papel y Márgenes:* Permite elegir tamaños normalizados (**A4, A3, A2, Carta, Legal**), orientación (Vertical/Horizontal), factor de escala y definir márgenes independientes milimétricos con botones de ajuste rápido (`Sin Marg.` y `Prefijados`).
* *Modo de Color y Resolución:* Modos de salida en **Color**, **Escala de grises** (para ahorrar tinta en imprenta) o **Monocromo** (alto contraste), con selector varidable desde **300 DPI** por defecto.


* **Pestaña 2: Configuración de Estrellas (General y Específica)**
* *Panel General:* Establece la magnitud límite global del firmamento (ej. $6.50$) y el umbral de etiquetado con tipografía general.
* *Panel Específico (`SpecificAstroRule`):* Lista administrable (`Agregar`, `Eliminar`, `Limpiar`) para aislar y destacar astross (como Sirio, Venus o las Pléyades) y asignarlestetiquetas de magnitudes independientes y tipografías personalizadas en negrita (**B**) o cursiva (*I*).


* **Pestaña 3: Configuración de Gráficos**
* *Información del Plano:* Campos para **Título** y **Subtítulo** de la investigación arqueológica con selección de fuente y tamaño.
* *Elementos Gráficos:* Casillas de verificación para activar la **Leyenda de magnitudes**, **Leyenda espectral**, **Leyenda de cielo profundo** **Línea del horizonte** , **Vía Láctea**y la **Flecha del Norte**.


* **Sección Inferior Fija (Controles Globales)**
* `Mostrar vista previa`: Abre una ventana flotante (`PreviewDialog`) para validar la lámina en memoria antes de guardarla.


* `Exportar`: Invoca el despachador de guardado para serializar el archivo final.


* `Cancelar`: Cierra el diálogo de manera segura.


* *Barra de Estado y Progreso:* Notifica en tiempo real la fase de extracción y renderizado.





---

### 2.2 Configuración de una Exportación Paso a Paso

Para generar una lámina cartográfica rigurosa, sigue este flujo de trabajo estándar:

1. **Ajusta el entorno en Stellarium:** Define las coordenadas geográficas de tu ubicación (latitud, longitud, altitud) y fija la fecha de interés, activa o desactiva las cuadriculas ecuatoriales y azimutales, agrega la eclíptica de fecha o J2000. activa lostipos de etiquetas de nombres comunes o nombre de cátalo.

2. **Inicia el módulo:** Abre el diálogo de `SkyChartExporter` desde la barra de herramientas(Debes tener todo ya configurado en stellarium)


3. **Configura el soporte físico (Pestaña 1):** Selecciona el formato (**PDF** o **PNG** ), el tamaño de hoja (**A4/A3**) y el modo de color (**Color, Escala de Grises o Monocromo**).
4. **Filtra la densidad celeste (Pestaña 2):** Ajusta la magnitud límite para el cielo de fondo y agrega en la sección **Específico** los astros que deseas resaltar.
5. **Añade la información cartográfica (Pestaña 3):** Escribe el título del de la carta o plano, y activa o desactiva  el horizonte local, la rosa de los vientos, leyenda de magnitud o tipo espectral.
6. **Comprueba y exporta:** Pulsa **Mostrar vista previa** para verificar la distribución y haz clic en **Exportar** para guardar el archivo definitivo.



---

### 2.3 Interpretación y Composición de la Lámina Cartográfica

El resultado exportado no es un gráfico plano convencional, sino una composición cartográfica estructurada:

* **Área Útil y Marco Perimetral :** El lienzo celeste se ajusta automáticamente dentro de los márgenes físicos respetando la escala y proporciones angulares reales.
* **Rosa de los Vientos Dinámica:** Ubicada en la esquina superior derecha, la flecha norte calcula el ángulo cartográfico real respecto al centro de proyección, señalando el Norte verdadero del horizonte del observador.
* **Leyendas Analíticas:**
* *Leyenda de Magnitudes:* Muestra una escala graduada de círculos que decrecen armónicamente ($r = 4.0 \times 0.82^m$) para identificar visualmente el brillo estelar.
* *Leyenda Espectral:* Muestra la barra cromática de las clases Harvard ($O, B, A, F, G, K, M$) adaptada al modo de color activo.
* *Leyenda de astros:* Muestra todos los objetos del cielo profundo, así como asterismos y vía láctea.


* **Pie de Página Técnico:** Documenta automáticamente el nombre del yacimiento, coordenadas geográficas, altitud, fecha y tipo de coordenadas de cuadricula exacto de la simulación.

---

## 3. Metodología Científica del Plugin

`SkyChartExporter` procesa la realidad astronómica mediante algoritmos geométricos y leyes físicas de representación desacopladas:

### Jerarquía de Brillo y Cuerpos Hiperbrillantes

* **Estrellas Convencionales ($M_v \ge -1.5$):** El radio de los discos estelares ($r$) se calcula mediante la función psicofísica:

$$r = \text{clamp}\left(0.18, \; 0.85 \times \sqrt{(\Delta m)^{1.15}}, \; 5.5\right) \times \text{escala}$$



garantizando que las estrellas tenues no desaparezcan y que las brillantes no saturen el plano[cite: 10].
* **Cuerpos Hiperbrillantes (Sol, Planetas y Luna):** Para astros más brillantes que Sirio ($-1.5$), el motor aplica `bodyRadiusForMagnitude()`, introduciendo una tasa de crecimiento lineal sobre la base estelar que permite al Sol o a la Luna dominar la escena sin desbordar el encuadre cartográfico.

### El Caso Especial de la Fase Lunar y Orientación Solar

* **Vector de Iluminación Tridimensional:** El extractor calcula en tiempo real la posición tridimensional del Sol respecto al observador (`sunDirection`)[cite: 4, 8].
* **Terminador Cartográfico:** La función `drawMoonPhase()` orienta el limbo iluminado de la Luna directamente hacia el Sol astronómico y traza la curvatura de la sombra mediante curvas de Bézier cúbicas moduladas por la fracción de iluminación real, permitiendo comprobar visualmente alineamientos lunares sobre santuarios.

### Cuadrículas de Coordenadas y Corte de Horizonte Topográfico

* **Muestreo Dinámico:** Las mallas de coordenadas Ecuatoriales ($RA/Dec$) y Horizontales ($Az/Alt$) se calculan con un espaciado adaptativo (desde $15^\circ$ hasta $1^\circ$) según el campo visual ($FOV$).
* **Interpolación en el Horizonte:** Cada segmento lineal que cruza la línea de $Alt = 0^\circ$ es truncado matemáticamente mediante `isAboveHorizon()`, garantizando que ningún astro o línea invisible bajo el terreno aparezca en el plano.

---

## 4. Estructura de Formatos y Exportación

El plugin integra motores de serialización especializados según la finalidad del documento:

### Perfil Imprenta y Edición Vectorial (PDF / SVG)

* **PDF Vectorial (`PdfExporter`):** Utiliza `QPdfWriter` a una resolución base de $72\text{ pt/in}$. Genera trazos continuos, polígonos puros y conserva los nombres de los astros como **texto vivo editable**, permitiendo modificar fuentes en programas de maquetación.
* **SVG Estándar (`SvgExporter`):** Implementa el algoritmo post-proceso `clipSvgFileToPageRect()` que inyecta etiquetas `<clipPath>` estándar en el XML, solucionando las limitaciones de recorte de Qt y garantizando plena compatibilidad en Inkscape e Illustrator.

### Perfil Digital y Publicación Web (PNG / JPG)

* **Mapas de Bits a 300 DPI (`ImageExporter`):** Emplea `QImage` para renderizar imágenes raster de alta densidad.
* **Corrección de Doble Escalado Tipográfico:** Durante el dibujo, fija temporalmente la densidad de la imagen en $72\text{ DPI}$ para que la geometría y los textos (`QFont`) se escalen armónicamente con `painter.scale()`, reasignando los metadatos físicos reales a 300 DPI justo antes de guardar el archivo[cite: 5].

---

## 5. Mejores Prácticas

* **Publicaciones en Blanco y Negro:** Utiliza el modo **Monocromo** en la Pestaña 1 para transformar todas las líneas y estrellas en tinta negra pura sobre fondo blanco, asegurando máxima nitidez en memorias astronomicas impresas.
* **Encuadernación de Informes:** Utiliza el botón `Prefijados` en márgenes para reservar $15\text{ mm}$ en los laterales de la lámina, evitando que el cosido o anillado del informe tape la información cartográfica.

* **Ajuste de fecha:** Verifica siempre que la fecha y hora de Stellarium correspondan a la cronología de la ubicación de estudio para asegurar la correcta reconstrucción por precesión del firmamento.

---

## 6. Créditos

* **Desarrollo Principal:** Asmat Vásquez.
* **Versión del Plugin:** 26.1.0
* **Agradecimientos:** A la comunidad de desarrolladores y al equipo fundador de Stellarium.

---

## 7. Licencia

Copyright (C) 2026 SkyChartExporter Developers

Este plugin es software libre; puede redistribuirlo y/o modificarlo bajo los términos de la **Licencia Pública General de GNU (GNU GPLv2 o posterior)** tal como la publica la Free Software Foundation.
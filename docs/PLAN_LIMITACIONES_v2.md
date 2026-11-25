# Plan de limitaciones — v2.0.0

> Este archivo es un resumen ejecutivo del análisis de limitaciones
> detectado en v1.0.0 y del plan para abordarlas. El análisis completo,
> con justificación técnica, opciones de diseño, esfuerzo estimado y
> riesgos, está en el documento
> `download/Plan_Limitaciones_v2_ext_images_src.docx` (entregado al
> usuario fuera del repositorio) y se referencia desde aquí.

## Limitaciones detectadas en v1.0.0

1. **Solo 8 bits por canal.** AVIF/JXL/HEIC pueden llevar 10/12/16 bits y
   HDR de coma flotante; la extensión siempre produce RGB/RGBA de 8 bits.
2. **Sin EXIF / ICC.** La orientación y los perfiles de color se ignoran.
3. **Solo primer fotograma** de AVIF y JXL animados.
4. **Sin límite de memoria/megapíxeles** antes de decodificar.
5. **Errores genéricos** (`ERR_PARSE_ERROR`) sin mensaje subyacente.

## Plan v1.1.0 (seguridad y robustez)

- Tope configurable de megapíxeles (por defecto 268 MP, igual que WebP en
  Godot) comprobado **antes** de asignar el buffer de salida.
- Tope configurable de tamaño de archivo antes de leerlo a memoria.
- Registrar el mensaje de error de cada biblioteca (`avifResultToString`,
  `heif_error_get_string`, estado de `JxlDecoder`) con `ERR_PRINT`.
- Harness de fuzzing que alimente archivos crafted a cada decodificador.

## Plan v1.2.0 (fidelidad) — COMPLETADO

- ✔ Ruta de 16 bits (y HDR float) para AVIF y JXL: `RGBH`/`RGBAH` para
  fuentes float (half float), `RGB16`/`RGBA16` para enteros de 10/12/16
  bits, conmutada por `extended_images/hdr_output`.
- ✔ Aplicar orientación EXIF en AVIF (irot/imir), JXL (orientation 1-8) y
  HEIC (propiedades irot/imir del item).
- ✔ Animaciones: clase `ExtendedImageInfo` con todos los fotogramas,
  duraciones por fotograma (segundos), número de bucles y flags HDR/alpha.
- ✘ Preservar el perfil ICC: queda pendiente (los píxeles salen en el
  espacio de color de la fuente). Moverlo a v1.3.0.

## Plan v1.3.0 (empaquetado)

- Reducir tamaño del binario (eliminar rutas de encoder de libjxl que no
  se usan, compilar libsharpyuv estáticamente).
- Build Linux universal que funcione entre distros.
- Enviar a la Godot Asset Library.
- Conversión de perfil ICC al espacio de color de la escena.

## Trazabilidad

Cada limitación del listado anterior se mapea a una sección del documento
de análisis `Plan_Limitaciones_v2_ext_images_src.docx` (sección
"Limitaciones detectadas" → "Análisis técnico" → "Diseño de la solución"
→ "Esfuerzo y prioridad" → "Riesgos y mitigaciones"). El documento
incluye, además de lo anterior:

- Diagrama de bloques de la arquitectura actual y de la propuesta.
- Pseudocódigo de cada cambio.
- Casos de prueba por implementar.
- Métricas de aceptación.

El presente archivo `PLAN_LIMITACIONES_v2.md` se mantiene en el
repositorio como referencia pública del roadmap; el `.docx` se entrega
aparte porque contiene gráficos y tablas que no son apropiadas para
Markdown.

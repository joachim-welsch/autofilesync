# AutoFileSync

[English](README.md) | [Deutsch](README.de.md) | **Español** | [Français](README.fr.md) | [Italiano](README.it.md) | [Polski](README.pl.md) | [Українська](README.uk.md)

AutoFileSync mantiene sincronizados los archivos de dos lugares: un solo
archivo, una carpeta o todo un árbol de carpetas. Cada lugar puede ser una
carpeta del propio ordenador, una unidad de red, una unidad USB, un ordenador
accesible por SSH o un móvil Android. Configurar, iniciar, olvidarse.

AutoFileSync es software libre bajo la GNU General Public License, versión 3
o posterior, y funciona en Windows, macOS y Linux.

Sitio web: <https://welsch-software.com>
Código fuente e informes de errores: <https://github.com/joachim-welsch/autofilesync>

## Funciones

- Sincronización de archivos, carpetas y árboles de carpetas, acotada con
  comodines.
- Comparación por fecha, tamaño o contenido; mover en lugar de copiar;
  tratamiento configurable de los archivos eliminados y de los archivos más
  recientes en el destino.
- Sincronización bidireccional que recuerda el último estado: las
  eliminaciones y los cambios de nombre se trasladan, y los archivos
  modificados en ambos lados se conservan como copias de conflicto.
- Una vista previa que muestra lo que haría una ejecución antes de que cambie
  nada.
- No se pierde nada: las generaciones guardan versiones anteriores, las
  eliminaciones van a la papelera, una unidad desconectada nunca se interpreta
  como «todo eliminado», y las eliminaciones masivas solo se ejecutan tras
  confirmarlas en la vista previa.
- Origen o destino por SSH en un NAS, un servidor o un ordenador de la red
  doméstica; las contraseñas se guardan cifradas.
- Los móviles Android se vinculan con un código QR; las plantillas guardan las
  fotos en el PC, mantienen una carpeta sincronizada o llevan música al móvil.
  La app (`AFS_Android/`) sincroniza en segundo plano, si se desea solo con
  wifi y mientras se carga.
- Los cambios se aplican al instante: las carpetas locales se vigilan en lugar
  de comprobarse solo a intervalos fijos.
- Un servicio en segundo plano que sincroniza con la misma configuración,
  incluso sin ningún usuario conectado.
- Un asistente integrado que responde preguntas sobre el programa a partir de
  la ayuda, explica por qué un destino no está al día y propone las órdenes
  adecuadas.
- Un registro con nivel seleccionable, aviso por correo electrónico ante
  errores graves y un informe de copia de seguridad en formato JSON.
- Interfaz y ayuda en inglés, alemán, español, francés, italiano, polaco y
  ucraniano; el idioma se elige en los ajustes.

## Instalación

Hay paquetes listos para Windows, macOS y Linux en
<https://welsch-software.com>. Para compilarlo uno mismo, consulte
`doc/BUILD.md`; la instalación completa, incluido el servicio en segundo
plano, se describe en `doc/INSTALL-Linux.md` y `doc/INSTALL-macOS.md` (estos
documentos están en alemán).

Resumen para Linux:

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libcurl4-openssl-dev libssl-dev \
                 imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Estructura

| Carpeta | Contenido |
|---|---|
| `AFS_Core/` | Núcleo de sincronización como biblioteca estática, sin interfaz |
| `AFS_Desktop/` | Programa con interfaz wxWidgets, ayuda y plantillas de instalador |
| `backgroundsync/` | Servicio en segundo plano para Linux, macOS y Windows |
| `AFS_Android/` | App de Android (Kotlin), instrucciones de compilación en `AFS_Android/README.md` |
| `po/` | Catálogos de idiomas |
| `third-party/` | Textos de licencia de las bibliotecas utilizadas |

Pruebas: `scripts/run_Integrationtests.sh` para el núcleo,
`scripts/run_GUI_Tests.sh` para la interfaz; detalles en `doc/BUILD.md`.

## Colaborar

Los informes de errores y las sugerencias se envían como issue a
<https://github.com/joachim-welsch/autofilesync/issues>; los cambios llegan
como pull request. Las contribuciones quedan bajo la misma licencia que el
proyecto (`doc/LICENSING.md`). El estilo de código y el procedimiento se
describen en `doc/CONTRIBUTING.md`.

## Apoyo

AutoFileSync es y seguirá siendo gratuito. Quien quiera apoyar su desarrollo
puede donar por PayPal a `joachim.welsch@posteo.de`. Las ampliaciones y
adaptaciones a medida se realizan con gusto a cambio de un pago; las
consultas, a `contact@welsch-software.com`.

## Licencia

GNU General Public License v3 o posterior, véase `LICENSE` y
`doc/LICENSING.md`. Copyright © 2026 Joachim Welsch. Los componentes de
terceros y sus licencias figuran en `doc/THIRD-PARTY-NOTICES.md`.

# C.L.T. no macOS

Empacota o **C.L.T.**, jogo da [Uniday Studio](https://uniday.studio/games/247-clt-early-access) em Acesso
Antecipado e só para Windows, num `CLT.app` que abre no macOS com dois cliques, sem CrossOver nem máquina virtual.

O repositório **não contém o jogo**. Ele guarda só o script que monta o app a partir do seu `CLT.exe`, comprado
na loja da Uniday. O `.exe` é de propriedade da Uniday Studio e está no `.gitignore`: não faça commit dele nem o
redistribua.

## Como funciona

O `CLT.exe` roda sem nenhuma modificação. O app traz junto as camadas de tradução:

```mermaid
flowchart LR
    exe["CLT.exe<br/>(x86-64, Windows, intacto)"] --> wine["Wine 11.21<br/>APIs Win32 sobre macOS"]
    wine --> rosetta["Rosetta 2<br/>x86-64 → arm64"]
    exe -- "Direct3D 11" --> dxmt["DXMT 0.80<br/>D3D11 → Metal"]
    dxmt --> metal["Metal / GPU Apple"]
```

| peça | origem | papel |
|---|---|---|
| Wine 11.21 | [Gcenx/macOS_Wine_builds](https://github.com/Gcenx/macOS_Wine_builds) | Win32, janela, input, áudio, rede e vídeo (Media Foundation via GStreamer) |
| DXMT 0.80 | [3Shain/dxmt](https://github.com/3Shain/dxmt) | Direct3D 11 traduzido para Metal |
| `winemac.so` corrigido | `packaging/wine-patches/` sobre o fonte do Wine 11.21 | expõe ao DXMT a view Metal da janela (ver abaixo) |
| Rosetta 2 | macOS | executa o código x86-64 em Apple Silicon |

Como o binário não é alterado, **atualizar o jogo é só trocar o `.exe`** (ver "Atualizar o jogo").

### O patch no `winemac.so`

O DXMT procura no driver de macOS do Wine a tabela `macdrv_functions`, que só existe nos builds do CrossOver. Sem
ela, a tela fica preta e o log mostra `Failed to create metal view`. O patch tem dois arquivos:

- `dxmt_export.c` exporta a tabela e entrega ao DXMT os dados da janela no layout do CrossOver. Usa a content view
  da janela, porque o Wine 11 só cria uma view de cliente para GL/Vulkan.
- `dxmt_cocoa.m` põe a view Metal acima da superfície GDI da janela, que no Wine 11 fica por cima e esconderia
  cada frame.

O `build.sh` compila só esse módulo, com o mesmo fonte do Wine que gerou o resto do pacote.

## Requisitos

- Mac com Apple Silicon e macOS 13 ou mais novo.
- Rosetta 2: `softwareupdate --install-rosetta --agree-to-license`.
- Para compilar: Xcode Command Line Tools e Homebrew com `brew install mingw-w64 bison`.
- Cerca de 4 GB livres para downloads, fonte do Wine e o app.

## Montar o app

```bash
mkdir -p references && cp /caminho/do/CLT.exe references/CLT.exe
packaging/build.sh
open build/CLT.app
```

Na primeira execução, o `build.sh`:
1. baixa o Wine e o DXMT;
2. clona o Wine 11.21 e compila o `winemac.so` com o patch;
3. monta `build/CLT.app` com o ícone tirado do próprio `.exe`.

As execuções seguintes reaproveitam o que já está em `vendor/` e levam alguns segundos.

Para instalar, arraste `build/CLT.app` para `/Applications`. Na primeira abertura o app cria o prefixo do Wine em
`~/Library/Application Support/CLT/prefix`, o que leva uns 30 segundos. Saves, configurações e cache de shaders
ficam nesse prefixo, fora do app.

## Atualizar o jogo

Quando sair uma versão nova do C.L.T., baixe o `.exe` novo e rode:

```bash
packaging/build.sh --exe /caminho/do/CLT-novo.exe
```

Também dá para trocar o arquivo direto no app: `CLT.app/Contents/Resources/game/CLT.exe`. Os saves continuam no
prefixo.

## Opções

| variável | padrão | efeito |
|---|---|---|
| `CLT_MAX_FPS` | `60` | limite de quadros, controlado pelo Metal; precisa dividir a taxa da tela: 30, 60 ou 120. `0` desliga o limite |
| `WINEDEBUG` | `-all` | logs do Wine, por exemplo `fixme-all` para diagnóstico |
| `MTL_HUD_ENABLED` | — | `1` mostra o HUD do Metal: FPS, GPU e confirmação de que o DXMT está ativo |

Como passar as variáveis:

```bash
open --env CLT_MAX_FPS=120 build/CLT.app
open --env MTL_HUD_ENABLED=1 build/CLT.app
```

Os logs ficam em `~/Library/Application Support/CLT/`: `clt.log` (Wine) e `CLT_d3d11.log` (DXMT).

## Estrutura

| caminho | conteúdo |
|---|---|
| `packaging/build.sh` | monta `build/CLT.app` (downloads, compilação do `winemac.so`, montagem e ícone) |
| `packaging/launcher.sh` | executável do app: cria o prefixo, aplica o limite de FPS e abre o jogo |
| `packaging/wine-patches/winemac.drv/` | patch do driver macOS do Wine para o DXMT |
| `packaging/brand_loader.py` | troca "Wine" por "CLT" no nome do app na barra de menus |
| `packaging/make_icon.py` | gera o `.icns` a partir do ícone do `.exe` |
| `packaging/Info.plist` | metadados do bundle |

## Estado

Testado num MacBook Pro M2 Max com macOS 26.6.2 e C.L.T. 0.5.0, em tela cheia a 1728×1117:

- abertura, tela de boas-vindas, menus e geração de mapa aleatório (Pequeno) funcionam;
- no jogo, HUD, minimapa, NPCs, trânsito e polícia aparecem, e mouse e teclado respondem (o celular abre com
  `SETA CIMA`);
- a renderização sai pelo Metal via DXMT, a 60 FPS travados, contra ~95–100 FPS sem limite;
- o C.L.T. procura atualização sozinho ao abrir, e o acesso à rede passa pelo WinHTTP do Wine.

Não verificados ainda: áudio (não foi ouvido durante o teste), controle (gamepad), partidas longas, mapas
Gigante/Colossal e o botão "Enviar Feedback".

C.L.T. © Uniday Studio. Wine é LGPL-2.1. DXMT 0.80 é MIT. Os scripts deste repositório são de uso pessoal para rodar
uma cópia legítima do jogo.

# Validação da v0.1 — 18/11/2026

**Imagem construída e smoke headless `core` aprovado no ipt-sim-01. A GUI falhou.**
Os resultados do IPT abaixo vêm dos logs e saídas enviados pelo usuário. O Work
continua sem Docker/display para repetir o cenário completo.

## Ambiente disponível

Ambiente de desenvolvimento Debian 13, x86_64, com Python, GCC e cabeçalhos obtidos
do fork no commit-base. Não havia Docker Engine, socket Docker, Podman ou Buildah.
O Compose standalone oficial v5.8.0 foi baixado apenas para validar a configuração;
seu SHA256 foi conferido contra o manifesto da release:

```text
b8840f5008c447f41fce0a57ae3adeab42c89b1eb6e6012524bcabbfb4ffbaf6
```

## Executado e aprovado

| Verificação real | Resultado e limite |
|---|---|
| Testes Python | 11 testes aprovados: conversão float32/64; mapa truncado; resolução NaN; ocupação infinita; mapa desconhecido sem corredor; dimensões/payload incompatíveis; mapa real; parâmetros sem alterar fonte; processo com erro; timeout com encerramento; processo bem-sucedido. |
| Mapa real | 237 × 210, 49.770 células e resolução preservadas exatamente na conversão; corredor de ensaio encontrado. |
| Sintaxe Python | `compileall` nos scripts e testes, sem erro. |
| Sintaxe Bash | `bash -n` nos quatro scripts shell, sem erro. |
| Auxiliares C | `gcc -std=c11 -D_DEFAULT_SOURCE -fsyntax-only -Wall -Wextra -Werror` com headers autênticos do fork: `map_bridge.c` e `ipc_probe.c` aprovados. Não é link nem execução. |
| Compose headless | `config --quiet` aprovado pelo Compose v5.8.0. |
| Compose X11 | `config --quiet` aprovado com DISPLAY/caminho de cookie de teste; não houve conexão a um display. |
| GtkGLExt | SHA256 e seis patches conferidos localmente; compilação e instalação também confirmadas no log do primeiro build no IPT. |
| Arquivos | YAML interpretado; rede isolada/capabilities/montagens conferidas; `git diff --check` sem erro. |

Comandos locais de validação que não exigem Docker Engine, executados na raiz do repo:

```bash
python3 docker/tests/test_infrastructure.py
python3 -m compileall -q docker/scripts docker/tests
for script in docker/scripts/*.sh; do bash -n "$script" || exit; done
git diff --check
```

Com Docker Compose instalado, a validação de esquema pode ser repetida sem construir:

```bash
docker compose -f docker/compose.yaml config --quiet
DISPLAY=:0 CARMEN_XAUTH=/tmp/carmen-schema-only.xauth \
  docker compose -f docker/compose.yaml -f docker/compose.x11.yaml config --quiet
```

O caminho fictício do segundo comando só serve para validar o YAML. Para execução
gráfica, utilize `run-x11.sh`, que cria um cookie real e temporário.

## Primeiro build real enviado pelo IPT

O usuário enviou o log `Eingefügter Text(1).txt` (1.601.153 bytes), SHA256
`3e2146e82638211acd26b60fdf363577389a9390efb3cf6db311128b36532d48`.
Ele contém várias tentativas com a mesma falha de link.

- Base Ubuntu26 baixada, apt concluído, GtkGLExt compilado/instalado e configure concluído.
- `phase1` concluída; `central`, `param_daemon` e `proccontrol` linkados.
- Primeiro bloqueio: `cannot find -lroute_planner_interface` e
  `cannot find -ltask_manager_interface` no link de `simulator_ackerman`.
- O `exit: Illegal number: -1` é consequência do tratamento de erro no Makefile.
- Nessa primeira tentativa a imagem final não foi gerada. A tentativa posterior,
  após a correção das interfaces, concluiu o build e passou no smoke descrito abaixo.

## Correção e teste real das interfaces

As seis bibliotecas de `docker/interfaces` foram compiladas e instaladas em uma
árvore temporária no Work com GCC 14.2, `-Wall -Wextra -Werror` e headers autênticos
do fork. O teste foi linkado também por G++, verificando os símbolos C usados pelos
consumidores C++. Não foram usados archives vazios, mocks de IPC ou symlinks de ABI.

Foi compilado o `central` e o IPC do próprio fork, junto às unidades C de global e
param_interface necessárias ao teste. O Work recusa `socket(AF_UNIX)` com EPERM;
por isso **apenas esse build de teste nativo** utilizou a opção já existente
`NO_UNIX_SOCKETS` para comunicar por TCP local. A configuração do container não foi
alterada por essa limitação do Work.

Resultado real de `make -C docker/interfaces CARMEN_HOME=<árvore-de-teste> install test`:

```text
PASS: IPC roundtrip mask=0x7ff expected=0x7ff (11 mensagens)
PASS: tipo de reboque não suportado recusado explicitamente
```

O roundtrip cobre road network, engage, configuração sem reboque, offroad plan,
Frenet paths, audit status, destino, rota predefinida, geometria de colisão e duas
mensagens de missão. Strings, arrays de trajetória e campos numéricos são conferidos
após serialização. Trata-se de mensagens sintéticas para testar transporte, sem executar
os planejadores privados. O runner impõe prazos, detecta saída prematura do central e
o encerra ao finalizar. A primeira tentativa com socket Unix recusado falhou corretamente.

Depois de concluir o build no IPT, repita no ambiente de destino:

```bash
cd ~/carmen_lcad-docker
docker compose -f docker/compose.yaml run --rm --entrypoint bash carmen -lc \
  'cd /opt/carmen/docker/interfaces && make interface_roundtrip && python3 ../tests/test_interfaces.py --central /opt/carmen/bin/central --probe ./interface_roundtrip --logs /logs/interfaces'
```

Esse resultado valida as interfaces adicionadas. Não comprova a simulação, a GUI,
a ABI dos prebuilt nem o link completo do CARMEN sob GCC15.

## Build e execução posteriores confirmados no IPT

Os logs posteriores enviados pelo usuário confirmaram compilação completa, inspeção
ELF/`ldd` dos nove binários e exportação da imagem `carmen-lcad:ubuntu26-v0.1`:

```text
Image carmen-lcad:ubuntu26-v0.1 Built
```

O comando `docker compose -f docker/compose.yaml run --rm carmen --mode core --smoke`
produziu no IPT, no ensaio `/logs/20261118T224533.995115Z`:

```text
PASS: IPC, mapa, processos estáveis, deslocamento e parada verificados.
```

O teste seguinte com X11, ensaio `20261118T224647.415918Z`, falhou. O log enviado
`Eingefügter Text(4).txt` contém reinícios de `navigator_gui2` por SIGNAL code 11
(SIGSEGV). O GTK chegou a carregar a interface e emitiu avisos de propriedades,
radio menu e widgets inválidos; esses avisos não localizam, por si, a instrução
que causou a falha. O `demo.py` recusou continuar ao detectar reinício de PID na
fase `observe`, antes da fase `drive`. `globalpos=0` é esperado no modo `core`.

Foi acrescentado diagnóstico opt-in de instância única, usando a imagem existente,
com captura de instrução/pilha e simbolização. Dois testes nativos passaram: falhas
SIGSEGV intencionais em executáveis PIE e não-PIE foram capturadas, preservaram o
sinal de saída e apontaram `crash_here` na linha 2; saída normal não foi marcada
como crash. Os 11 testes de infraestrutura continuam aprovados, assim como as
sintaxes Python/Bash. **Isso valida o capturador, não a execução gráfica.**

```bash
python3 docker/tests/test_gui_diagnostics.py
bash docker/scripts/diagnose-x11.sh  # no IPT, com a imagem já construída e display
```

O diagnóstico posterior do IPT (`gui-diagnostic-20261118T225753.902968Z`)
localizou a falha em `Speedometer::update(float)`, chamada por
`base_ackerman_odometry_handler` durante `ConfigureMenu`/consultas IPC.
O fonte revelou que `checkArguments` só criava os instrumentos dentro de `argc > 1`.
Sem argumentos, o construtor assinava odometria com `speedometer` nulo.

Correção: `initializeComponents()` passou a executar incondicionalmente, antes
de qualquer consulta de parâmetros ou assinatura IPC. A seleção do tópico padrão
(odometria) e dos tópicos escolhidos por `-type_msg` foi preservada.
Um teste C++ da lógica real de startup, com sentinelas no lugar de IPC/instrumentos,
passou em cinco variantes de argumentos e falhou com o código anterior. Os 13
testes anteriores também passaram. O teste não executa GTK/OpenGL nem substitui
o novo build e ensaio X11 no IPT. A correção exige reconstruir a imagem.
Nenhum aviso do GTK foi suprimido e nenhuma falha de PID foi ignorada para aprovar
o cenário. A execução gráfica após a correção ainda não foi confirmada.

## Estado por etapa

| Etapa | Estado |
|---|---|
| Download da base e instalação apt dentro da imagem | Confirmado pelo log do IPT |
| Compilação GtkGLExt/configure/phase1 na imagem | Confirmada pelo log do IPT |
| Link dos executáveis na imagem | Concluído no IPT após correção das interfaces |
| Inspeção ELF e `ldd` dos nove binários finais | Aprovada no build do IPT |
| Inicialização de central e param_daemon no cenário | Aprovada pelo smoke `core` no IPT |
| Comunicação real mapa/truepos/odometria/heartbeat/proccontrol | Aprovada pelo smoke `core` no IPT |
| Movimento e parada do veículo simulado | Aprovados pelo smoke `core` no IPT |
| Filtro localize_ackerman por laser 2D | Integração experimental implementada, não executada |
| Janela navigator_gui2, mapa e veículo visíveis | SIGSEGV localizado no painel; fonte corrigido, novo ensaio pendente |
| Navegação autônoma até uma meta | Cadeia de controle ainda não concluída |

A imagem local e o core estão validados no escopo do smoke. Localização, visualização
e navegação completa continuam pendentes; a GUI não deve ser considerada funcional.

## Critério de aceitação no ipt-sim-01

Siga os comandos completos de build, smoke e X11 no [README.md](README.md).
Registre o commit, o digest da base, o manifesto apt e os logs do mesmo ensaio.

1. Build retorna zero; todos os binários prioritários passam pela inspeção ELF/loader.
2. `--mode core --smoke` retorna zero e grava `result.json` com `PASS`.
3. `--mode localized --smoke` também passa, exigindo `globalpos` recente.
4. X11 exibe mapa e veículo; `demo.py` produz deslocamento visível e parada.
5. Só depois integrar/validar a cadeia navegador → controlador → simulador e testar
   metas, colisões e erro de localização em cenários com resultados esperados.

O probe requer várias mensagens com timestamps novos, processos ativos e PIDs estáveis,
além de deslocamento e parada no modo drive. Falha, timeout, reinício ou ausência de
mensagens geram saída não zero; iniciar um processo por si só não aprova o cenário.

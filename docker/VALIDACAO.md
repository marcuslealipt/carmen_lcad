# Validação da v0.1 — 18/11/2026

**A implementação ainda não tem validação funcional de CARMEN em container.**
Esta distinção é intencional: teste estático aprovado não demonstra execução do veículo.

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
| GtkGLExt | SHA256 do tarball conferido; os seis patches aplicaram sem erro ao fonte extraído. Não houve compilação da biblioteca. |
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

## Não executado / permanece pendente

| Etapa | Estado |
|---|---|
| Download da base e instalação apt dentro da imagem | Não executado |
| Compilação GtkGLExt/GCC15/CARMEN na imagem | Não executado |
| Link, inspeção ELF e `ldd` dos nove binários finais | Verificação implementada, não executada |
| Inicialização de central e param_daemon | Não executada |
| Comunicação real mapa/truepos/odometria/heartbeat/proccontrol | Smoke implementado, não executado |
| Movimento e parada do veículo simulado | Smoke implementado, não executado |
| Filtro localize_ackerman por laser 2D | Integração experimental implementada, não executada |
| Janela navigator_gui2, mapa e veículo visíveis | Não executado; exige display do host |
| Navegação autônoma até uma meta | Cadeia de controle ainda não concluída |

As duas alterações em fontes CARMEN foram revisadas, mas esses módulos completos não
foram compilados aqui. O container pode revelar dependências transitivas, parâmetros
adicionais ou incompatibilidades de runtime. Não há resultado `PASS` funcional produzido
por esta sessão nem imagem publicada como pronta.

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

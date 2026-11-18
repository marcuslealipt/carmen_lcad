# CARMEN Docker v0.1 — experimento Ackermann do IPT

Fork: `marcuslealipt/carmen_lcad`, branch `feat/docker-ubuntu26`.
Destino: `ipt-sim-01`, Ubuntu 22.04.5 x86_64, Docker Engine + Compose.
Container: Ubuntu 26.04, CPU, sem CUDA. Não há instalação de dependências CARMEN no host.

**Estado desta entrega: infraestrutura implementada; build da imagem e simulação ainda
não executados.** O ambiente de desenvolvimento não disponibiliza Docker Engine.
Os testes executados e seus limites estão em [VALIDACAO.md](VALIDACAO.md).
Não há afirmação de que a GUI, a localização ou a navegação completa já funcionem.

## O que a v0.1 prepara

- Imagem de desenvolvimento com GCC 15, dependências apt e GtkGLExt 1.2.0 compilado.
- Bibliotecas do conjunto ativo upstream; executáveis prioritários de IPC, parâmetros,
  processos, simulação, localização, planejamento e GUI.
- Cenário `core`: `central`, `param_daemon`, `proccontrol`, ponte de mapa e
  `simulator_ackerman`. Comandos limitados permitem testar movimento real do simulador.
- Cenário `localized`: acrescenta `localize_ackerman`, com laser 2D sintético,
  odometria do simulador e mapa estático. O teste exige mensagens `globalpos` novas.
- `--gui`: acrescenta `navigator_gui2`, usando X11/XWayland local e renderização por CPU.
- `--planner`: inicia também `navigator_ackerman` para investigação da cadeia de
  planejamento. **Ainda não fecha a navegação autônoma até uma meta da GUI.**

O movimento de demonstração é um comando de velocidade/esterçamento enviado por IPC,
integrado pelo `simulator_ackerman`. Não há log de veículo sendo reproduzido.
O perfil utiliza geometria Pioneer de 0,75 × 0,39 m e entre-eixos de 0,56 m;
não representa ainda o IARA completo nem um caminhão.

## 1. Baixar e construir no ipt-sim-01

Os comandos pressupõem que Docker Engine e o plugin Compose já estejam disponíveis
para seu usuário. Execute num terminal Bash do `ipt-sim-01`:

```bash
docker version
docker compose version

git clone --depth 1 --single-branch --branch feat/docker-ubuntu26 \
  https://github.com/marcuslealipt/carmen_lcad.git ~/carmen_lcad-docker
cd ~/carmen_lcad-docker
mkdir -p docker/logs

export LOCAL_UID="$(id -u)"
export LOCAL_GID="$(id -g)"
export SOURCE_REVISION="$(git rev-parse HEAD)"
export BUILD_JOBS=2

docker compose -f docker/compose.yaml config --quiet
set -o pipefail
docker compose --progress plain -f docker/compose.yaml build carmen \
  2>&1 | tee docker/logs/build-host.log
```

O clone raso evita baixar todo o histórico. Ainda é um repositório grande. O contexto
Docker exclui os mapas de campanhas, logs e pesos neurais não usados neste cenário.
Em caso de falta de memória na compilação, repita o build com `export BUILD_JOBS=1`.
Não é necessário alterar o sistema-base do host, instalar ROS ou instalar CUDA.

Se o diretório já existe, não repita o clone: confira `git status`, faça `git fetch origin
feat/docker-ubuntu26` e use `git switch feat/docker-ubuntu26` sem descartar mudanças locais.

## 2. Validar progressivamente

Execute na raiz `~/carmen_lcad-docker`, depois de concluir o build:

```bash
# ELF x86_64 e dependências dinâmicas dos nove binários essenciais.
docker compose -f docker/compose.yaml run --rm --entrypoint bash carmen \
  /opt/carmen/docker/scripts/check-binaries.sh

# Teste headless: IPC, parâmetros, mapa, processos, movimento e parada.
docker compose -f docker/compose.yaml run --rm carmen --mode core --smoke

# Acrescenta localização real por laser 2D; exige globalpos contínuo.
docker compose -f docker/compose.yaml run --rm carmen --mode localized --smoke
```

Cada execução cria `docker/logs/<data-hora-UTC>/`. O resultado funcional é registrado
em `result.json`, com `PASS` ou `FAIL`; um erro retorna código não zero. A ausência de
`result.json` também não é aprovação. O supervisor mantém logs separados de cada módulo,
inclusive erros que normalmente ficariam escondidos pelo `proccontrol`.

O smoke test aguarda IPC e parâmetros, confirma a pose inicial recebida do simulador,
observa seis segundos de comunicação, comanda **0,2 m/s em linha reta durante seis
segundos**, envia parada e observa novamente. Exige deslocamento entre 0,2 e 1,8 m,
velocidade final menor que 0,05 m/s, timestamps recentes, contagens mínimas e PIDs
estáveis. O corredor inicial foi validado para esse ensaio. O teste não mede a precisão
da localização nem demonstra chegada a uma meta de navegação.

Não há publicação de portas, rede do host, dispositivos de veículo ou acesso CAN.
Todos os módulos compartilham um container com rede `none` e IPC em localhost.

## 3. Abrir o mapa e observar o veículo

Use uma sessão gráfica **local** do `ipt-sim-01` com `DISPLAY=:N` e cookie X11
acessível ao `xauth` já disponível no host. O wrapper recusa SSH X11 forwarding com
`DISPLAY=localhost:N`, pois esse modo exigiria uma configuração de rede diferente.

Terminal 1:

```bash
cd ~/carmen_lcad-docker
bash docker/scripts/run-x11.sh
```

O wrapper copia temporariamente o cookie da sessão para um arquivo com modo 600,
monta apenas esse arquivo e o socket X11 como somente leitura e remove a cópia ao sair.
Não usa `xhost`, `--privileged`, `/dev/dri` nem GPU. O UID/GID da imagem precisa coincidir
com seu usuário; isso é definido no build do passo 1. X11 ainda concede à aplicação
acesso à sessão gráfica autorizada; use uma sessão de pesquisa confiável.

Terminal 2, enquanto a GUI permanecer aberta:

```bash
docker exec carmen-x11 python3 /opt/carmen/docker/scripts/demo.py
```

A demonstração reinicializa o veículo no corredor livre, confirma a comunicação e
realiza um movimento curto com parada. Pode ser repetida: a pose inicial é restaurada.
A observação visual da janela continua sendo necessária para validar a renderização.
`xdpyinfo` confirma acesso ao display, não confirma que mapa e veículo foram desenhados.

Depois de validar `localized --smoke`, a variante gráfica com localização é:

```bash
cd ~/carmen_lcad-docker
bash docker/scripts/run-x11.sh --mode localized --gui
```

Encerre com Ctrl+C no primeiro terminal. O supervisor encerra seus grupos de processos;
`--rm` remove o container, preservando os logs. Os fontes do checkout não são montados
por cima dos binários compilados na imagem.

## 4. Investigar o planejador

```bash
cd ~/carmen_lcad-docker
bash docker/scripts/run-x11.sh --mode localized --gui --planner
```

Essa opção é experimental. O `navigator_ackerman` do commit-base publica
`motion_planner_path`; o cenário antigo não contém toda a cadeia necessária para
converter isso em comandos atuais do simulador. O perfil não inicia SDKs de veículo
nem planejadores privados. A v0.1 não declara navegação autônoma completa. O teste
de comando direto é bloqueado quando `--planner` está ativo, para evitar fontes
concorrentes de comando. O estado do processo do planejador é monitorado, mas sua
correção e produção de trajetórias ainda precisam de um teste específico.

## Configuração e estrutura

| Item | Decisão |
|---|---|
| Contexto do build | Raiz do repositório; exclusões em `/.dockerignore` |
| CARMEN_HOME | `/opt/carmen` |
| Diretório de execução | `/opt/carmen/bin`, preservando os caminhos relativos |
| Configuração gerada | `/run/carmen/carmen.ini` e `/run/carmen/process.ini` |
| Mapa convertido | `/run/carmen/ctvii.map` |
| Mapa original | `data/mapa_do_ctvii_500k_64.map`, preservado |
| Logs persistentes | `docker/logs` no host → `/logs` no container |
| Dados futuros | `/dados` existe, mas esta v0.1 não requer dataset externo |
| Usuário | UID/GID numéricos definidos no build; sem root no runtime |
| Ambiente gráfico | Overlay `docker/compose.x11.yaml`, cookie e renderização por CPU |
| Build | `./configure --nojava --nocuda --noqt3` com respostas vazias e validação das flags |

`prepare.py` materializa um único perfil `[*]`, usando os parâmetros atuais do Ford
como base, geometria/offsets do Pioneer e overrides explícitos. Não modifica `.bashrc`
nem os `.ini` upstream. A geometria de colisão experimental usa três círculos de raio
0,25 m; ainda exige avaliação para planejadores. A calibração dinâmica de caminhões
fica para as próximas versões.

A fase `export` e as bibliotecas `phase1` seguem a lista **ativa** do `src/Makefile`.
Isso é mais amplo que sete módulos porque suas interfaces dependem de mapper,
grid_mapping, rddf, sensores, libtf, prob_models, carmodel, libcontrol etc.
`phase2` é executada apenas para `ipc`, `global`, `maptools`, `param_daemon`, `proccontrol`,
`simulator_ackerman`, `localize_ackerman`, `navigator_ackerman`, `navigator_gui2` e os
auxiliares Docker. Alguns desses diretórios produzem também executáveis de teste.
Nenhum módulo comentado no `PACKAGES` é adicionado automaticamente.

A imagem retém ferramentas, fontes e bibliotecas de desenvolvimento. Isso conserva
os symlinks absolutos produzidos pelos Makefiles e facilita a próxima iteração;
ainda não é uma imagem runtime mínima.

## Reprodutibilidade e ABI

O tarball GtkGLExt tem SHA-256 verificado; os seis patches usam o commit fixo
`917d7c6ff529b1d290d8c834bf67bd5e69d2f9b1` do pacote Deepin.
Não se usa `lib/compat` nem alias entre versões diferentes de Python/GSL/GTK.
Somente `prebuilt/ubuntu26-x86_64` entra no contexto; é preservado inclusive
`libtrailer_nlp.so`, mas planejadores privados não são iniciados no cenário básico.
O teste de loader reprova bibliotecas ausentes nos executáveis essenciais.

O tag `ubuntu:26.04` e os pacotes apt podem receber atualizações. Portanto o build
ainda não é bit a bit reprodutível. Para fixar a imagem-base de um ensaio:

```bash
docker pull ubuntu:26.04
export UBUNTU_IMAGE="$(docker image inspect ubuntu:26.04 --format '{{index .RepoDigests 0}}')"
export SOURCE_REVISION="$(git rev-parse HEAD)"
docker compose --progress plain -f docker/compose.yaml build carmen
docker image inspect carmen-lcad:ubuntu26-v0.1 > docker/logs/image-inspect.json
docker compose -f docker/compose.yaml run --rm --entrypoint cat carmen \
  /opt/carmen/build-logs/dpkg-packages.tsv > docker/logs/dpkg-packages.tsv
```

Além desses registros, a imagem contém `build-logs/build.log` e `source-revision.txt`.
Fixar também o snapshot apt é melhoria futura. A base **não** foi trocada para Ubuntu
20.04: não houve ainda um build que demonstrasse inviabilidade do 26.04.

## Diagnóstico e testes locais

```bash
cd ~/carmen_lcad-docker
python3 docker/tests/test_infrastructure.py

# Logs do último ensaio (escolha o diretório mostrado pelo supervisor).
ls -lt docker/logs

# Shell de diagnóstico, com o mesmo usuário e ambiente da imagem.
docker compose -f docker/compose.yaml run --rm --entrypoint bash carmen
```

Dentro do shell, por exemplo:

```bash
cd /opt/carmen/bin
ldd ./simulator_ackerman
ldd ./localize_ackerman
ldd ./navigator_gui2
cat /opt/carmen/src/Makefile.vars
cat /opt/carmen/build-logs/source-revision.txt
```

Se o build falhar, conserve `docker/logs/build-host.log` e corrija a primeira falha
real. Se o smoke falhar, veja `simulator.log`, `map_bridge.log`, `localize.log`,
`proccontrol.log`, `observe.log` e `drive.log` no diretório daquele ensaio.
Não interprete apenas `Spawned` do proccontrol ou `ELF/loader OK` como simulação aprovada.

Referências técnicas: [investigação do fork](INVESTIGACAO.md),
[tutorial Ubuntu 26.04](../doc/migracao_ubuntu26/install/INSTALL_UBUNTU_26.04.md),
[contexto Docker](https://docs.docker.com/build/concepts/context/),
[serviços Compose](https://docs.docker.com/reference/compose-file/services/).

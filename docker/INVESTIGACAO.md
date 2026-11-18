# Investigação técnica — CARMEN Docker v0.1

Data: 18/11/2026. Base examinada: `2fb13e417739e00b85b7fe069442c372b124b95f`,
no fork `marcuslealipt/carmen_lcad`. As observações de runtime nos documentos antigos
são relatos daquela migração, **não testes executados nesta entrega Docker**.

## Fontes examinadas

- `doc/migracao_ubuntu26/CHANGELOG.md`
- `doc/migracao_ubuntu26/AUDIT_DEPENDENCIAS.md`
- `doc/migracao_ubuntu26/install/INSTALL_UBUNTU_26.04.md`
- `doc/migracao_ubuntu26/install/05_pendencias_conhecidas.md`
- `src/Makefile`, `src/Makefile.conf`, `src/Makefile.rules`, `src/configure`
- `bin/process-simulate-ackerman.ini`, `bin/process-carmen-simulation.ini`
- `src/carmen-pioneer-ackerman.ini`, `src/carmen-ford-escape.ini`
- Fontes, mensagens IPC e Makefiles de simulador, localizador, navegador, GUI,
  proccontrol, param_daemon, maptools e bibliotecas relacionadas.

## Dependências e fronteira do build

| Componente | Dependência/decisão |
|---|---|
| Toolchain | GCC/G++ 15, make, CMake, pkg-config, flex/byacc e autotools. O script rejeita outro major de GCC para não variar silenciosamente o perfil. |
| IPC, parâmetros e processos | Bibliotecas `ipc`, `global`, interfaces e utilitários do próprio CARMEN; zlib, pthread, math e bibliotecas de sistema. |
| Simulador Ackermann | Interfaces CARMEN, geometria, colisão, `libcarmodel`, `libcontrol`, FANN e GSL, mesmo com redes neurais desativadas em runtime. |
| Localização | `prob_models`, PCL, Eigen, OpenCV, Bullet/libtf, GSL, OpenMP e bibliotecas de mapas/sensores. Desativar sensores 3D em runtime não elimina as dependências de link. |
| GUI | GTK2, OpenGL/Mesa, GtkGLExt, libtf e várias interfaces. GTK3 permanece para o proccontrol_gui e outros alvos do conjunto ativo. |
| GtkGLExt | Sem pacote apt segundo a auditoria do fork. Tarball GNOME 1.2.0 com SHA256 conferido e seis patches fixados no commit `917d7c6ff529b1d290d8c834bf67bd5e69d2f9b1` do pacote Deepin. |
| Boost | Usa a migração existente para Signals2; System é header-only. Não instala Boost 1.61 nem recria `libboost_system.so` artificialmente. |
| OpenCV | Usa OpenCV 4 e `pkg-config opencv4` da árvore migrada; nenhum OpenCV 3 paralelo. |
| PCL | A auditoria registra 1.15 e exigência C++17; os Makefiles migrados são preservados. |
| Python | Bindings CARMEN desativados pelo configure; headers/lib de Python do sistema continuam necessários a bibliotecas e GUI que incorporam Python. |
| Qt | Qt3 desativado. O proccontrol_gui atual é GTK3; Qt5 é mantido no conjunto apt de desenvolvimento/dependências transitivas, sem tentar portar a GUI antiga. |
| Prebuilt | Somente `prebuilt/ubuntu26-x86_64`; preserva interfaces/libs necessárias, inclusive dependência CasADi 3.7. Binários privados não participam do cenário. |

`apt-packages.txt` é a lista efetivamente solicitada pelo Dockerfile. As versões exatas
instaladas serão registradas em `/opt/carmen/build-logs/dpkg-packages.tsv` pelo build.
Versões citadas na auditoria não substituem esse manifesto. Ainda falta executar apt,
compilar e linkar na imagem limpa.

`build.sh` executa `export` e `phase1` na lista **ativa** de `PACKAGES` e `PACKAGES_LIB`
do `src/Makefile`, incluindo `carmenpp` e excluindo bindings Java/Python. Isso preserva
a ordem e as numerosas interfaces compartilhadas. Não percorre arbitrariamente todos
os diretórios históricos. A base lista pouco mais de 80 módulos; nem todos viram processos.

A fase de executáveis (`phase2`) é chamada apenas nos diretórios:

```text
ipc global maptools param_daemon proccontrol simulator_ackerman
localize_ackerman navigator_ackerman navigator_gui2
```

Os Makefiles desses diretórios também podem produzir utilitários auxiliares declarados
nos respectivos `TARGETS`; não há promessa de gerar somente sete executáveis. Os dois
programas de `docker/tools` são compilados em seguida. A inspeção ELF exige os sete
binários prioritários e esses dois auxiliares. A árvore-fonte permanece na imagem porque
o build original instala vários arquivos via symlinks para ela.

## Automação do configure

Comando obrigatório preservado:

```bash
./configure --nojava --nocuda --noqt3
```

O script fornece uma entrada finita de 12 linhas em branco e impõe timeout de 120 s.
Isso conserva os defaults recomendados pelo tutorial: IPC interno, C++ habilitado,
bindings Python desativados, driver de laser padrão e prefixo de instalação padrão.
Headers opcionais ausentes aceitam o default sem instalar SDKs proprietários.
Depois verifica `NO_JAVA=1`, `NO_CUDA=1`, `NO_PYTHON=1`, gráficos/C++ habilitados,
Qt3 desativado e `CARMEN_HOME=/opt/carmen`. Perguntas adicionais ou um default alterado
devem ser tratados como incompatibilidade de build, não por um `yes` infinito.

## Cenários legados e mapa

`process-simulate-ackerman.ini` é o ponto de partida mais próximo: contém simulador,
localização, navegação, GUI e o mapa pequeno existente no repositório. Também pede
`robot_ackerman_gui`, comentado no `PACKAGES` atual. O perfil novo não o executa e não
altera o `.ini` original.

`process-carmen-simulation.ini` utiliza mapa de campanha
`map_ida_guarapari-20170403-2`, RDDF e uma cadeia mais ampla de behavior selector,
planejamento e controle. Esse mapa não está entre os mapas disponíveis na raiz de
`data/` inspecionada. Não é um teste inicial autossuficiente.

O arquivo `data/mapa_do_ctvii_500k_64.map` existe, porém o sufixo `_64` é enganoso:
seu grid usa **float32**, enquanto o leitor atual de maptools lê **double**. A inspeção
binária constatou 237 × 210 células, resolução `0.10000000149011612` m e 49.770 valores.
`prepare.py` valida o tamanho dos chunks e converte apenas o grid para float64 no
diretório gerado. Todas as células e a resolução foram comparadas exatamente após a
conversão. O mapa original permanece intacto. Metadados legados fora do grid não são
transportados para o arquivo de simulação.

O corredor livre selecionado começa em aproximadamente `(1,6; 5,4; 0)`.
A busca verifica ocupação conhecida abaixo de 0,2, folga lateral/traseira de 0,4 m e
2 m à frente. Isso serve ao teste limitado da geometria Pioneer, não a um veículo maior.

## Integração experimental

1. Parâmetros recentes do Ford fornecem as chaves novas requeridas pelo código atual.
   Apenas geometria e offsets específicos vêm do Pioneer; overrides desligam sensores
   3D, redes neurais, hardware e MPC. O gerador materializa um perfil único com última
   definição prevalecendo, sem executar includes ou comandos dos arquivos originais.
2. O simulador atual recebe `offline_map`; somente iniciar param_daemon com um arquivo
   de mapa não satisfaz esse protocolo. `docker_map_bridge` lê o grid convertido e
   publica `offline_map` e `mapper_map` periodicamente.
3. O simulador agora aguarda o primeiro mapa antes de integrar o estado/ray-casting.
   Isso evita operar sobre resolução zero durante a inicialização.
4. `localize_ackerman -static_map` é um caminho opcional que gera o mapa de likelihood
   pela rotina existente. Exige `mapping_mode off`, laser bruto e predição por odometria.
   Sem a opção, permanece o fluxo original de mensagens do map_server.
5. O teste inicializa a pose via IPC e exige confirmação em truepos. Para mover, publica
   o estado `Free_Running` e comandos Ackermann, depois velocidade zero e `Stopped`.
6. `navigator_ackerman` publica caminhos para uma cadeia de controle mais ampla.
   O `.ini` antigo não comprova que essa cadeia ainda fecha no código atual.
   Por isso `--planner` é experimental e separado do smoke de comando direto.

Os perfis não usam playback. Há integração de comandos no simulador e geração de
odometria/laser/truepos. A aceitação do filtro de localização exige publicação contínua,
mas ainda não mede erro de posição nem convergência estatística.

## ABI e pendências conhecidas

- Não são criados symlinks entre Python 3.8/3.14, GSL 23/28, GTK2/GTK3 ou Boosts
  incompatíveis. `lib/compat` não entra no contexto nem em `LD_LIBRARY_PATH`.
- `task_manager` e `fastslam` legados têm dependências antigas/privadas descritas na
  documentação de migração. Não são necessários aos perfis desta entrega.
- `route_planner`, `offroad_planner` e outros prebuilt não são validados por existir
  arquivo ou por resolver símbolos. A versão Ubuntu26 é preservada como fornecida;
  qualquer uso futuro exige validação de ABI e comportamento.
- FlyCapture2, libwnn, MAE e drivers físicos continuam opcionais/fora do cenário.
  Os Makefiles ativos já limitam alguns desses módulos às interfaces quando falta SDK.
- Diretórios inativos, terceiros CUDA/Caffe e bridges ROS não são migrados aqui.
- Qt3 e `robot_ackerman_gui` antigos não são reativados. O aviso de canberra GTK2
  citado na migração não deve ser tratado com symlink para a versão GTK3.
- Docker compartilha o kernel do host: a imagem Ubuntu26 não altera o Ubuntu22.04,
  mas sua compatibilidade efetiva depende do teste no Engine/kernel do `ipt-sim-01`.

Ubuntu26 permanece a base porque existe migração específica no fork e nenhuma falha
de build real desta imagem foi observada — o build não pôde ser executado. Não houve
troca para Ubuntu20.04. Se uma ABI privada obrigatória impedir o perfil, avaliar um
Dockerfile separado para 20.04 com bibliotecas reais daquela distribuição, sem misturar
os prebuilt de versões diferentes. Primeiro deve-se tentar recompilar a dependência
com fonte disponível e conservar o log da primeira falha.

# CONTINUAR AQUI — Lenneth / vpl_nx no R36S

> **Checkpoint de desenvolvimento, atualizado em 10/11/2026 (horário BR).**
> Este arquivo é a referência de retomada. É importante atualizar **este arquivo**
> e o [PROMPT_RETOMADA_CHATGPT.md](PROMPT_RETOMADA_CHATGPT.md)
> **a cada avanço**, antes de encerrar uma sessão ou entregar um pacote de teste.

## 1. Identidade correta do projeto

- Repositório de trabalho: https://github.com/dehalbuq/ValkyrieRecomp
- **Branch ativa:** `port/lenneth-vplnx-linux-arm64-loader`
- Código do projeto neste ramo: `vplnx-r36s/`
- Base de estudo/adaptação MIT: https://github.com/delsonazevedo/vpl_nx
- **Alvo:** Valkyrie Profile: **Lenneth**, usando o motor **Android ARM64** original
  que o proprietário fornece em privado (`libMyGame.so` e
  `libc++_shared.so`), no R36S **rk3326 AArch64**, firmware **dArkOSRE**.
- **Não confundir** com a recompilação de Valkyrie Profile PS1 nem com as
  antigas Fases 30–43 do outro carregador JNI.
- Objetivo futuro: motor nativo via Android/Bionic/JNI compatibility layer,
  Mali-G31 acelerada, controles físicos, áudio estável, 4:3 quando possível,
  possibilidade de pular vídeos introdutórios **sem corromper lógica/saves**.
- O `vpl_nx` original **ainda usa o motor binário Android**. Não é uma
  decompilação integral do jogo.
- Não há jogo executando neste ramo: **não chamar JNI_OnLoad comercial,
  inicializadores ou nativeRender antes da conclusão dos bloqueios de ABI**.

## 2. Último diagnóstico REAL do R36S — Teste 5

**Recebido em 10/11/2026**: `VPLNX_R36S_DIAGNOSTICOS.tar(3).gz` (3.886 bytes).
Pasta interna: `execucao-20250904-080307-$` — o relógio do dArkOSRE
**está incorreto**, registrando setembro de 2025. Usar a data de
recebimento e CI para ordenar os testes; não inferir que foi executado
em 2025.

**Boa notícia:** após a correção de snapshots atômicos, **todos os onze
arquivos de texto da execução têm tamanho não zero**, inclusive
`ELF_REAL_IMPORTS.log` (7.710 bytes), `ELF_REAL_BASIC.log`
(2.472 bytes), `execucao.txt` (6.175 bytes), `resultados.tsv`
e `resumo.txt`. `FINAL=FINISHED`. **Isso confirma uma coleta
completa, mas ainda não prova recuperação após queda de energia**.

| Teste | R36S | Evidência |
|---|---|---|
| PTHREAD_BIONIC_ABI | **PASS** | mutex 40, cond 48 bytes, sincronização |
| HOST_SAFE | **PASS** | allowlist de Bionic sem liberações indevidas |
| GLES2_GPU | **PASS** | SDL KMSDRM, ARM Mali-G31 |
| GLES3_GPU | **PASS** | OpenGL ES 3.2, GPU real |
| OPENSL_AUDIO_FISICO | **PASS** | 44.100 Hz estéreo, callback ativo |
| JNI_ASSETS | **PASS** | JNI 1.6, strings/arrays, arquivo de teste |
| ELF_REAL_IMPORTS | **BLOCKED** | vinculação real incompleta |
| ELF_REAL_BASIC | **BLOCKED** | exports 4/4 presentes, sem host imports |

Hardware capturado: Linux **4.4.189**, **aarch64**, Debian **13/trixie**
(dArkOSRE), **916800 kB de RAM**, **sem swap**, DRM card0/renderD128,
GPU ARM **Mali-G31**, driver GLES **3.2 v1.r13p0-01rel0**,
controle **GO-Super Gamepad** detectado.

### Contagem exata das relocações sem provedor

| Módulo | Relocações pendentes | Nomes distintos |
|---|---:|---:|
| `libMyGame.so` | **59** | **57** |
| `libc++_shared.so` | **63** | **62** |
| **Total** | **122** | **99 no conjunto**, 20 comuns |

Comparação: diagnóstico **Teste 3 = 85 + 98 = 183** relocações
pendentes. O Teste 5 tem **61 relocações a menos**, após alterações
na allowlist POSIX. Não extrapolar esse ganho para FPS ou jogo jogável.

`BIONIC_HOST_IMPORTS accepted=281 blocked=122`.
`guest host_resolved=211`, `partner host_resolved=70`.
`unsupported=0`, `relr=0`, mas `MEMORY_PROTECTION=BLOCKED`.
`ELF_REAL_BASIC` reporta 270+133 faltantes intencionalmente
**porque NÃO usa a tabela de imports do host**; não somar esses valores
às 122 relocações ainda pendentes na análise principal.

**Lista COMPLETA e priorização**:
[PENDENCIAS_TESTE5.md](PENDENCIAS_TESTE5.md).

Nenhum construtor ou `JNI_OnLoad` do motor comercial foi executado.
O wrapper só manipula as bibliotecas reais em memória para análise
e não as incluiu no arquivo de diagnóstico.

## 3. Estado atual do código

O ramo contém:
- `src/platform_linux.[ch]`: SDL2 KMSDRM / EGL / GLES2 e GLES3, entrada de gamepad
- `src/elf_linux.[ch]`: carregador ELF64 AArch64, RELA, símbolos entre
  módulos, W^X, fail-closed para imports ausentes
- `src/host_imports_linux.[ch]`: allowlist de Bionic/libc, NDK assets,
  OpenSL, GLES e parte de POSIX sem `FILE*` ou caminhos livres
- `src/bionic_fortify_linux.[ch]`: wrappers FORTIFY de strings, leitura e
  printf verificados; `src/bionic_fortify_check.c` testa aborts por overflow;
  ainda exige validação de compatibilidade com as bibliotecas reais.
- `src/bionic_pthread_linux.[ch]`: mutex/cond Android de 40/48 bytes,
  mapeados para objetos pthread privados do Linux; ainda não inclui
  `pthread_create`, `pthread_rwlock`, TLS ou atributos avançados
- `src/assets_linux.[ch]`: arquivos grandes por streaming com proteção
  de diretórios
- `upstream/jni_fake_linux.c`: JNI 1.6 fake do `vpl_nx` adaptado;
  serviços Android não implementados falham, em vez de simular sucesso
- `upstream/opensles.c`: áudio OpenSL ES sobre SDL2
- `PortMaster/run-r36s-diagnostico.sh`: testes de hardware e
  `ELF_REAL_IMPORTS` com bibliotecas comerciais locais, sem executá-las
- `.github/workflows/vplnx-r36s-platform.yml`: compilação ARM64,
  testes sintéticos e ZIP PortMaster (sem arquivos proprietários)

**Teste 6 de compatibilidade Bionic/FORTIFY (após os logs do Teste 5):**
adicionadas `src/bionic_fortify_linux.[ch]` e
`src/bionic_fortify_check.c`. São seis wrappers de fortificação
(`__strcat_chk`, `__strncpy_chk`, `__strchr_chk`,
`__read_chk`, `__vsnprintf_chk`, `__vsprintf_chk`),
além de funções wchar/tipos compatíveis adicionadas na allowlist
do host. Testes sintéticos de chamadas válidas e `SIGABRT` para
overflow passaram no [GitHub Actions 42941739463](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42941739463).
O ZIP PortMaster resultante possui **7 executáveis AArch64**.
**Nenhum teste foi executado com as bibliotecas comerciais após
essa mudança**; os números 59+63 ainda são a última contagem real.

**Último avanço de código antes deste checkpoint:** foi criada
persistência de diagnósticos por snapshots atômicos após cada teste,
com preservação do arquivo anterior:
- `VPLNX_R36S_DIAGNOSTICOS.tar.gz`: último snapshot válido;
- `VPLNX_R36S_DIAGNOSTICOS_ANTERIOR.tar.gz`: backup da execução anterior;
- `diagnosticos/execucao-.../`: pasta detalhada, se for necessário
  coletar manualmente;
- proteção contra colisões de nomes quando o relógio do portátil reseta.

**Validação do mecanismo de snapshots:** o GitHub Actions
[42906180629](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42906180629)
concluiu com **SUCCESS** em 10/11/2026, incluindo reexecução, backup de arquivo
anterior, pacote válido e simulação de teste individual. O ZIP do Teste 5
inclui seis binários ELF64 AArch64, sem conteúdo do jogo, e foi
inspecionado depois do build. **Ainda não foi testado o comportamento
de recuperação após queda de energia no R36S real.**

## 4. Regras de segurança e compatibilidade

1. Não publicar, anexar ao GitHub Actions público, nem armazenar no repo
   `libMyGame.so`, `libc++_shared.so`, ROMs, `.cpk`, APKs ou outros assets.
2. É permitido testar bibliotecas locais fornecidas pelo usuário no R36S,
   sem copiá-las ao relatório; incluir **somente logs de símbolos e erros**.
3. Nenhum offset de patch do Switch deve ser usado cegamente: o binário do
   usuário tem **endereços diferentes**, e a análise inicial encontrou
   **sete colisões definidas** entre os hooks do `vpl_nx` e outras funções.
   Preferir símbolos reais do ELF + validação da versão.
4. Não tratar `PASS` dos diagnósticos como jogo jogável.
5. Não mapear `FILE*`, `pthread_t`, `stat`, `DIR*`, permissões,
   callbacks ou TLS Bionic para glibc diretamente sem prova de ABI.
6. Testes de GPU no desktop CI são feitos com Mesa software: somente logs
   do dispositivo real comprovam aceleração Mali-G31.
7. O alvo de performance/skip intro só pode ser avaliado **depois** de
   inicialização real e perfis de CPU/GPU. Não acelerar lógica do jogo.

## 5. Próximas tarefas em ordem recomendada

1. **Teste 5 recebido e logs ELF completos.** Usar a
   [lista de 99 símbolos reais](PENDENCIAS_TESTE5.md), sem repetir
   testes que já passaram. Confirmar Teste 6 com o novo FORTIFY no
   hardware quando o CI e o pacote estiverem validados.
2. **Bionic FORTIFY e operações wchar básicas**: implementação de
   seis wrappers e da allowlist foi enviada ao GitHub. Validar regressões
   em ARM64 e medir a redução *real* das 122 relocações anteriores.
3. Implementar **arquivos/saves/stdio com objetos Bionic opacos** e
   caminhos confinados; não expor `FILE*` ou `open` ilimitado na glibc.
4. Adaptar **threads/TLS e lifecycle C++**:
   `pthread_create`, `pthread_key_*`, `__cxa_atexit`,
   `__cxa_finalize`, `__cxa_thread_atexit_impl`, unwind e
   `__stack_chk_guard`. `setjmp/longjmp` exigem ponte própria.
5. Quando **todas** as importações e APIs relevantes tiverem
   implementações seguras, avaliar `JNI_OnLoad` comercial em
   processo isolado com timeout/logs privados e sem uploads ao CI.
6. Depois: `nativeInit/nativeRender` com GLES2, FPS e controle,
   sincronização áudio, intro skip sem offsets fixos de outra build.

## 6. Como retomar em outro chat

Abra e copie:
**[PROMPT_RETOMADA_CHATGPT.md](PROMPT_RETOMADA_CHATGPT.md)**.

O novo assistente deve **abrir o código e workflow no GitHub, inspecionar
o último commit/CI**, e verificar qualquer ZIP que o usuário envie antes
de continuar. Não supor acesso a este chat.

## 7. Links de referência

- [Lista completa de pendências ELF Teste 5](PENDENCIAS_TESTE5.md)

- [Código ativo neste ramo](https://github.com/dehalbuq/ValkyrieRecomp/tree/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s)
- [Workflow de compilação ARM64](https://github.com/dehalbuq/ValkyrieRecomp/actions/workflows/vplnx-r36s-platform.yml)
- [CI aprovado, etapa Teste 4 POSIX (antes dos snapshots)](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42895373964)
- [CI aprovado, Teste 5 com snapshots e backup automático](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42906180629)
- [CI aprovado, Teste 6 FORTIFY ARM64](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42941739463)
- [Código upstream MIT vpl_nx](https://github.com/delsonazevedo/vpl_nx)

**Checklist de encerramento de cada sessão:** atualizar este estado,
atualizar o prompt, adicionar link do último CI real, registrar o que
foi testado em R36S vs em CI, e separar o próximo blocker comprovado.

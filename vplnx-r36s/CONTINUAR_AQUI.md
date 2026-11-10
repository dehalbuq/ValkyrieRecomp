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

## 2. Situação comprovada no R36S

**Última evidência recebida:** ZIP manual
`execucao-20250904-080230-3358.zip` recebido em **10/11/2026**.
**Atenção ao relógio:** o dArkOSRE registrou **04/09/2025**, aparentemente
o horário local do dispositivo reiniciado, e **não** a data da coleta real.
Use data de recebimento/commits/execuções do CI ao comparar.

A coleta manual contém `execucao.txt` completo e `resultados.tsv`, mas
`ELF_REAL_IMPORTS.log`, `ELF_REAL_BASIC.log`, `OPENSL_AUDIO_FISICO.log`,
`JNI_ASSETS.log` e `resumo.txt` têm **zero bytes** nesse ZIP.
Por isso os PASS/BLOCKED abaixo são confirmados pelo **log central**, mas
**não se pode extrair dele os números atualizados de relocações nem a lista
completa de imports pendentes**. Não utilizar números de outras execuções
como se fossem medidos neste ZIP.

| Teste | Resultado no ZIP manual | Evidência |
|---|---|---|
| PTHREAD_BIONIC_ABI | **PASS** | mutex 40 bytes, cond 48 bytes; ciclo de vida e cond_wait |
| HOST_SAFE | **PASS** | allowlist Bionic testada; ABI opaca não liberada |
| GLES2_GPU | **PASS** | SDL KMSDRM, GPU ARM Mali-G31 |
| GLES3_GPU | **PASS** | SDL KMSDRM, GPU ARM Mali-G31 |
| OPENSL_AUDIO_FISICO | **PASS** | SDL abriu 44100 Hz, 2 canais |
| JNI_ASSETS | **PASS** | JNI 1.6, arrays, strings, descritores de asset |
| ELF_REAL_IMPORTS | **BLOCKED** | não foi possível finalizar a vinculação |
| ELF_REAL_BASIC | **BLOCKED** | JNI exports 4/4, mas dependências faltam |

Hardware capturado: Linux **4.4.189**, **aarch64**, Debian **13/trixie**
(dArkOSRE), **MemTotal 916800 kB**, **SwapTotal 0**,
`/dev/dri/card0` e `renderD128`, EGL/GLES **OpenGL ES 3.2
v1.r13p0-01rel0** na **Mali-G31** real (não llvmpipe).
Controle **GO-Super Gamepad** detectado.

O log central mostra, entre as últimas funções ausentes do módulo
`libc++_shared.so`, `mbrlen`, `vfprintf`, `wcrtomb`,
`__cxa_atexit`. **Isso não é a lista completa de ausências**.
As chamadas da `libMyGame.so` e da `libc++_shared.so` ainda são
analisadas apenas **sem execução de código comercial**.

Para referência histórica, antes da ampliação POSIX (Teste 3),
haviam sido anotadas **85 relocações pendentes no motor e 98 no C++**.
**NÃO** declarar essas quantidades como contagem do Teste 4: os logs
individuais foram recebidos vazios.

## 3. Estado atual do código

O ramo contém:
- `src/platform_linux.[ch]`: SDL2 KMSDRM / EGL / GLES2 e GLES3, entrada de gamepad
- `src/elf_linux.[ch]`: carregador ELF64 AArch64, RELA, símbolos entre
  módulos, W^X, fail-closed para imports ausentes
- `src/host_imports_linux.[ch]`: allowlist de Bionic/libc, NDK assets,
  OpenSL, GLES e parte de POSIX sem `FILE*` ou caminhos livres
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

1. **CI dos snapshots aprovado** (run 42906180629). Próximo teste com
   o pacote Teste 5 no R36S deve confirmar que `DIAGNOSTICOS.tar.gz`
   sobrevive a reinicialização forçada e que `DIAGNOSTICOS_ANTERIOR.tar.gz`
   conserva a execução anterior; não reiniciar propositalmente durante
   teste comercial, e manter a pasta detalhada se houver dúvida.
2. **Recuperar ELF_REAL_IMPORTS.log completo**, preferencialmente por ZIP
   gerado com snapshots atômicos ou copiando a pasta detalhada depois da
   execução. Determinar imports que bloqueiam cada módulo e contagens novas.
3. Priorizar **ABI Bionic de arquivo/stdio e `__cxa_atexit`/destrutores**;
   estudar `pthread_create`, `pthread_rwlock`, TLS e unwind separadamente.
   Não habilitar `fopen`/file API direta sem paths de assets/saves isolados.
4. Ampliar os testes sintéticos do loader e impedir `mprotect(...,PROT_EXEC)`
   quando faltar qualquer resolução ou validação relevante.
5. Quando **todos** os blockers estiverem resolvidos, tentar
   `JNI_OnLoad` real em **processo isolado**, com timeout/logs e arquivos
   privados, seguido depois por EGL/contexto e `nativeInit/nativeRender`.
6. Medir FPS e identificar renderização ES2 vs ES3; só então estudar
   pular introduções com segurança e sem usar offsets de outra versão.

## 6. Como retomar em outro chat

Abra e copie:
**[PROMPT_RETOMADA_CHATGPT.md](PROMPT_RETOMADA_CHATGPT.md)**.

O novo assistente deve **abrir o código e workflow no GitHub, inspecionar
o último commit/CI**, e verificar qualquer ZIP que o usuário envie antes
de continuar. Não supor acesso a este chat.

## 7. Links de referência

- [Código ativo neste ramo](https://github.com/dehalbuq/ValkyrieRecomp/tree/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s)
- [Workflow de compilação ARM64](https://github.com/dehalbuq/ValkyrieRecomp/actions/workflows/vplnx-r36s-platform.yml)
- [CI aprovado, etapa Teste 4 POSIX (antes dos snapshots)](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42895373964)
- [CI aprovado, Teste 5 com snapshots e backup automático](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42906180629)
- [Código upstream MIT vpl_nx](https://github.com/delsonazevedo/vpl_nx)

**Checklist de encerramento de cada sessão:** atualizar este estado,
atualizar o prompt, adicionar link do último CI real, registrar o que
foi testado em R36S vs em CI, e separar o próximo blocker comprovado.

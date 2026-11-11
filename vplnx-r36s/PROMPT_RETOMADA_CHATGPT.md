# PROMPT DE RETOMADA — Lenneth no R36S

**Copie o texto abaixo para iniciar qualquer novo chat sobre este port.**
Ele deve permanecer atualizado junto com
[CONTINUAR_AQUI.md](CONTINUAR_AQUI.md).

---

Quero continuar exatamente de onde paramos no port nativo do
**Valkyrie Profile: Lenneth Android ARM64 para R36S (rk3326 / dArkOSRE)**,
baseado no wrapper de código aberto **vpl_nx**.

O repositório é:
https://github.com/dehalbuq/ValkyrieRecomp

**Ramo ativo obrigatório**:
`port/lenneth-vplnx-linux-arm64-loader`

**Antes de alterar qualquer coisa, leia estes arquivos públicos no GitHub:**
1. https://github.com/dehalbuq/ValkyrieRecomp/blob/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s/CONTINUAR_AQUI.md
2. https://github.com/dehalbuq/ValkyrieRecomp/blob/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s/README.md
3. https://github.com/dehalbuq/ValkyrieRecomp/blob/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s/PortMaster/run-r36s-diagnostico.sh
4. https://github.com/dehalbuq/ValkyrieRecomp/blob/port/lenneth-vplnx-linux-arm64-loader/.github/workflows/vplnx-r36s-platform.yml
5. https://github.com/dehalbuq/ValkyrieRecomp/blob/port/lenneth-vplnx-linux-arm64-loader/vplnx-r36s/PROMPT_RETOMADA_CHATGPT.md

Confira o **último commit do ramo e o estado atual do GitHub Actions**
antes de afirmar que algo compilou. Não retome uma versão anterior
por engano. Se houver novos arquivos de diagnóstico anexados a este chat,
inspecione **todo o conteúdo**, inclusive `execucao.txt`,
`resultados.tsv` e `ELF_REAL_IMPORTS.log`.

**Regra nova e prioritária do usuário (10/11/2026):**
FAÇA TODOS OS TESTES POSSÍVEIS NA NUVEM, SEM PEDIR TESTES DE
DIAGNÓSTICO NO R36S. Só voltaremos ao portátil quando houver uma
tentativa real de iniciar a engine. Mantenha a bateria CI
automatizada em cada push; nunca prometa trabalhar depois de
encerrar o turno sem uma automação real. Não publique assets
comerciais e não confunda a engine sintética com Lenneth.

**Últimos resultados da nuvem:**
- CI principal ARM64 PASS:
  https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42945365906
- CI profunda AArch64 release-stress + ASan/UBSan PASS:
  https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42945392281
- As duas bibliotecas ELF ARM64 **sintéticas** executaram
  `JNI_OnLoad` no Linux, interagindo com JNI 1.6, pthread, libc
  e funções C++ (`__cxa_atexit`/`__cxa_finalize`).
  Testes adicionais: 250 inicializações Release, 25 sob
  ASan/UBSan, 128 ELF corrompidos recusados.
  NENHUMA engine comercial foi executada.
- `tests/cloud_import_coverage.py` correlacionou os 99 nomes
  pendentes no último R36S com o código atual: 24 novos provedores,
  **75 nomes ainda pendentes**, sendo 48 no motor e 43 em libc++
  (existem nomes compartilhados). Isto é APENAS COBERTURA NOMINAL;
  **não afirmar nova contagem de relocações reais**.
- O workflow `vplnx-android-private.yml` foi criado em
  **dehalbuq/ValkyrieRecomp-build-private** para analisar os dois
  binários Android arm64 em GitHub Actions privado, quando
  disponíveis nos caminhos
  `android-lenneth/lib/arm64-v8a/libMyGame.so` e
  `android-lenneth/lib/arm64-v8a/libc++_shared.so`.
  Não publica esses arquivos. Se faltarem, resultado esperado
  WAITING_FOR_INPUTS. Não declarar o preflight real aprovado
  até verificar que rodou com as bibliotecas originais.

**Estado comprovado até 10/11/2026:**
- R36S real usa kernel 4.4.189 ARM64, dArkOSRE Debian 13,
  Mali-G31 com OpenGL ES 3.2 (driver r13p0) via SDL KMSDRM.
- GLES2 e GLES3 aprovados no hardware real.
- OpenSL ES/SDL2 a 44100 Hz, JNI 1.6, AAsset e adaptadores
  pthread/cond Bionic 40/48 bytes passaram nos testes individuais.
- As bibliotecas privadas `libMyGame.so` e `libc++_shared.so`
  são reconhecidas no R36S, mas sua vinculação **ainda está BLOQUEADA
  por imports Bionic e ABI incompleta**.
- O motor comercial **NUNCA foi inicializado** nesta versão: não declarar
  jogo rodando, FPS medido ou intro pulável.
- **Último diagnóstico é o Teste 5, recebido em 10/11/2026**:
  `VPLNX_R36S_DIAGNOSTICOS.tar(3).gz` com
  `ELF_REAL_IMPORTS.log` de 7710 bytes e demais logs COMPLETOS.
  Pasta interna `execucao-20250904-080307-$`, com data de 2025
  INCORRETA pelo relógio do R36S; usar data real de recebimento.
  Todos os testes individuais de hardware PASS, porém as bibliotecas
  comerciais ainda BLOCKED:
  * `libMyGame.so`: 59 relocações pendentes, 57 nomes únicos;
  * `libc++_shared.so`: 63 relocações pendentes, 62 nomes únicos;
  * 122 relocações; **99 nomes únicos totais (20 comuns)**.
  O comparativo anterior era 183 relocações, logo 61 a menos.
  A lista EXATA dos 99 nomes consta de
  `vplnx-r36s/PENDENCIAS_TESTE5.md`. **Não tratar
  `ELF_REAL_BASIC.log` como contagem real**: ele deliberadamente
  desativa a tabela host e reporta muito mais faltas.
- Os snapshots automáticos preservaram TODOS os logs do Teste 5
  finalizado; GitHub CI de backup/snapshots também PASSOU.
  Ainda NÃO foi testado o comportamento após queda de energia.
- **Após este diagnóstico**, foi integrada no ramo uma nova camada
  `bionic_fortify_linux.c` com 6 símbolos Android FORTIFY
  (`__strcat_chk`, `__strncpy_chk`, `__strchr_chk`,
  `__read_chk`, `__vsnprintf_chk`, `__vsprintf_chk`),
  além de funções wchar simples. O CI
  https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42941739463
  **PASSOU** (compilação e testes sintéticos ARM64).
  Essa camada ainda NÃO foi validada com os binários comerciais
  no R36S. Não inventar nova contagem resolvida.

- O código não publica nem baixa bibliotecas/ROM/CPK comerciais.
- **O vpl_nx é um wrapper Android e não um novo motor totalmente livre.**
- O APK Android do usuário não é binariamente idêntico ao do Switch:
  **jamais aplicar offsets/hook fixos** do `vpl_nx` sem resolver os
  símbolos ELF e verificar a versão.

**Próximo trabalho prioritário:**
1. Examinar `CONTINUAR_AQUI.md` e os últimos GitHub Actions
   antes de qualquer mudança. Manter branch
   `port/lenneth-vplnx-linux-arm64-loader`.
2. NÃO pedir novas execuções no R36S até a fase de testar a engine.
3. Se ainda não houver bibliotecas Android no **repo privado**,
   orientar como disponibilizá-las ali, com consentimento do usuário.
   Não mover automaticamente conteúdo comercial para repos públicos.
4. Executar e inspecionar **preflight ARM64 privado das libs reais**,
   medindo as pendências de cada módulo, sem inicializar o jogo.
5. Resolver imports/ABI restantes (stdio/saves, pthread/TLS,
   locale/C++), rodando testes sintéticos, stress, ASan/UBSan e
   adversarial ELF automaticamente em nuvem a cada commit.
6. Só após os bloqueios realmente resolvidos, preparar teste
   isolado de `JNI_OnLoad` comercial em ambiente privado de CI.
   Solicitar R36S somente para tentativa de execução da engine.
7. Atualizar este prompt e `CONTINUAR_AQUI.md` após cada etapa.

**Minha preferência de trabalho:** português brasileiro, acompanhamento
amigável e técnico, avanço contínuo com correções reais no GitHub,
diagnósticos automatizados, pacotes PortMaster quando precisarmos testar
o hardware. Não inventar sucesso nem arquivos. Prefiro testes em lote.

**REGRA PERMANENTE para você, assistente:** ao terminar cada etapa,
atualize **CONTINUAR_AQUI.md** com o que realmente foi feito,
teste/CI/branch/último commit, bloqueios e próximo passo. Atualize
também **PROMPT_RETOMADA_CHATGPT.md** se houver progresso ou mudança
material. Deixe um prompt atualizado e fácil de copiar na sua
última resposta para que eu continue em outro chat sem perder nada.

Comece por conferir o estado atual do repositório e o último resultado
dos testes e então avance diretamente no bloqueio comprovado mais
importante. Não peça que eu repita informação já registrada no GitHub.

---

**Fim do prompt.**

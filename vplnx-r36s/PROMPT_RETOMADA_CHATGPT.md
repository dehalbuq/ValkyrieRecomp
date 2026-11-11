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
1. Ler `vplnx-r36s/CONTINUAR_AQUI.md` e
   `vplnx-r36s/PENDENCIAS_TESTE5.md` antes de alterar código.
2. Validar o **Teste 6 FORTIFY** no R36S, conferindo logs completos
   `ELF_REAL_IMPORTS.log`; medir a diferença em relação aos 122
   blockers do Teste 5, sem confundir testes sintéticos com engine.
3. Implementar **shims ABI Bionic para arquivos/saves/stdio**, ciclo de
   vida C++ `__cxa_atexit/finalize`, threads/TLS e símbolos restantes;
   nunca passar `FILE*` e `pthread_t` diretamente para glibc.
4. Usar GitHub Actions ARM64 para validar por lotes. Não publicar
   arquivos comerciais no repositório.
5. Só testar `JNI_OnLoad` original isoladamente após completar as
   dependências com ABI segura. Nada de offsets hardcoded do Switch.

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

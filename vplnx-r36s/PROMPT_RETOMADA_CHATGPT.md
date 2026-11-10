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
- O último diagnóstico manual foi o ZIP
  `execucao-20250904-080230-3358.zip` recebido em 10/11/2026.
  O relógio do R36S estava incorreto (mostrou 2025).
  `execucao.txt` e `resultados.tsv` estavam completos, enquanto
  alguns logs individuais de ELF/JNI/áudio e `resumo.txt` tinham
  **0 bytes**. Os resultados individuais podem ser inferidos do
  log central, mas **NÃO** uma nova contagem exata de importações
  pendentes. Últimas linhas exibem `mbrlen`, `vfprintf`,
  `wcrtomb` e `__cxa_atexit` entre outras pendências no C++.
- Havia um problema: o arquivo automático de diagnóstico podia
  ficar vazio depois de interrupção/reinicialização. Foi implementada
  coleta por snapshots atômicos depois de cada teste, backup
  `VPLNX_R36S_DIAGNOSTICOS_ANTERIOR.tar.gz` e proteção contra
  colisões causadas pela data do sistema errada. Verificar CI/efeito
  no hardware antes de declarar isso aprovado no R36S.
- O código não publica nem baixa bibliotecas/ROM/CPK comerciais.
- **O vpl_nx é um wrapper Android e não um novo motor totalmente livre.**
- O APK Android do usuário não é binariamente idêntico ao do Switch:
  **jamais aplicar offsets/hook fixos** do `vpl_nx` sem resolver os
  símbolos ELF e verificar a versão.

**Próximo trabalho prioritário:**
1. Verificar se o último CI do ramo passou e se o pacote dos novos
   snapshots foi produzido corretamente.
2. Recuperar `ELF_REAL_IMPORTS.log` completo para calcular
   os bloqueios ATUAIS do motor e do libc++ separadamente.
3. Implementar os **shims ABI Bionic para arquivos/saves/stdio e
   lifecycle C++**, sem mandar FILE*/pthread_t diretamente à glibc;
   explorar TLS/threads adicionais apenas com testes sólidos.
4. Usar GitHub Actions ARM64 para testar por lotes e não pedir
   teste em R36S a cada pequena alteração.
5. Só tentar JNI_OnLoad comercial quando símbolos/ABIs forem
   resolvidos, isolando processo, timeout e logs; **sem enviar
   arquivos comerciais ao CI público**.

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

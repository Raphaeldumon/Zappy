// Conventional Commits config for wagoid/commitlint-github-action (ci.yml).
// Subjects like: feat: ..., fix(server): ..., chore: ...
module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'body-max-line-length': [0, 'always'],
  },
};

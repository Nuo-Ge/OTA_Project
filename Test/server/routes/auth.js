const { Router } = require('express')
const { login, me } = require('../controllers/auth')
const requireAuth = require('../middleware/requireAuth')

const router = Router()

router.post('/login', login)
router.get('/me', requireAuth, me)

module.exports = router
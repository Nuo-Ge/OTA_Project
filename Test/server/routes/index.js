const { Router } = require('express')
const { getStatus, startUpgrade, getShadow } = require('../controllers/ota')

const router = Router()

router.get('/status', getStatus)
router.post('/upgrade', startUpgrade)
router.get('/shadow', getShadow)

module.exports = router
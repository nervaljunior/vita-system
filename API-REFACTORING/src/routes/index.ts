import { Router } from 'express';
import UserController from '../controllers/UserController';
import AuthController from '../controllers/AuthController';


const router = Router();

router.post('/user/createUser',UserController.createUser);
router.post('/user/session', AuthController.authUser);

export default router;